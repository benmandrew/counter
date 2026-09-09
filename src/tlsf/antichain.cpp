#include "tlsf/antichain.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "bounded_async.hpp"
#include "filter/implication.hpp"
#include "fingerprint/lasso.hpp"
#include "thread_pool.hpp"
#include "tlsf/filter.hpp"
#include "tlsf/fingerprint_prefilter.hpp"

namespace tlsf {
namespace {

constexpr std::size_t k_none = static_cast<std::size_t>(-1);

struct Counters {
    std::atomic<std::size_t> m_solver_queries{0};
    std::atomic<std::size_t> m_refuted{0};
    std::atomic<std::size_t> m_short_circuited{0};
    std::atomic<std::size_t> m_reconciled{0};
};

// `m_forward` is "a implies b", and under this order that makes a the survivor:
// a is the stronger specification and b the one it dominates. The direction
// reads backwards from the word "maximal" and is the one
// tlsf_make_implication_filter documents, so the two sweeps agree.
struct Verdict {
    bool m_forward{false};
    bool m_backward{false};
};

enum class Relation : std::uint8_t {
    Incomparable,
    CandidateDropped,
    MemberRemoved,
    Equivalent,
};

// Read with the candidate as side a and the member as side b. Equivalence is
// returned rather than resolved so the caller counts each collapse once, at the
// point it applies it.
Relation relation_of(const Verdict& verdict) {
    if (verdict.m_forward && verdict.m_backward) {
        return Relation::Equivalent;
    }
    if (verdict.m_forward) {
        return Relation::MemberRemoved;
    }
    if (verdict.m_backward) {
        return Relation::CandidateDropped;
    }
    return Relation::Incomparable;
}

// Positions in the wave's snapshot, not arrival indices: the snapshot is what
// the scan saw, and the replay re-reads it against the antichain as it stands.
struct ScanResult {
    std::size_t m_dropped_by{k_none};
    std::vector<std::size_t> m_removes;
};

// One walk over the arrivals, in three steps a wave.
//
// Phase 1 scans every arrival of the wave against a snapshot of the antichain
// taken before the wave, each on its own worker. A stale snapshot is enough
// because both kinds of decision survive whatever the rest of the wave does to
// the antichain: a candidate dominated by `a` stays dominated when `a` is
// removed, by whatever removed `a`; and a member removed by `x` stays dominated
// when `x` is itself dropped, by whatever dropped `x`. Implication is
// transitive, so neither verdict can be undone.
//
// What a snapshot does not cover is two arrivals of the same wave that are
// comparable to each other. Phase 2 settles exactly those, as one parallel
// region over the wave's survivors.
//
// Phase 3 replays the wave in discovery order over the exact antichain, using
// the two phases' answers and asking the solver nothing. That is what makes the
// walk's output a function of the arrivals rather than of the wave size.
class Walk {
   public:
    Walk(const std::vector<Arrival>& arrivals,
         std::vector<fingerprint::PackedFingerprint> prints,
         const TlsfSimilarityKey& similarity, SatisfiabilityChecker& checker,
         AntichainRowCallback on_row)
        : m_arrivals(arrivals),
          m_prints(std::move(prints)),
          m_checker(checker),
          m_on_row(std::move(on_row)) {
        m_keys.assign(arrivals.size(), 0.0);
        for (std::size_t i = 0; similarity && i < arrivals.size(); ++i) {
            m_keys[i] = similarity(arrivals[i].m_specification);
        }
        m_present.assign(arrivals.size(), 0);
        m_rows.reserve(arrivals.size());
    }

    void run_wave(std::size_t begin, std::size_t end) {
        m_begin = begin;
        m_snapshot = m_antichain;
        scan_wave(end - begin);
        reconcile();
        replay(end - begin);
    }

    [[nodiscard]] std::vector<AntichainRow> take_rows() {
        return std::move(m_rows);
    }

    [[nodiscard]] const Counters& counters() const { return m_counters; }

   private:
    [[nodiscard]] const Specification& spec(std::size_t index) const {
        return m_arrivals[index].m_specification;
    }

    [[nodiscard]] const fingerprint::PackedFingerprint* print(
        std::size_t index) const {
        return m_prints.size() == m_arrivals.size() ? &m_prints[index]
                                                    : nullptr;
    }

    Verdict compare(std::size_t left, std::size_t right) {
        const fingerprint::PackedFingerprint* print_left = print(left);
        const fingerprint::PackedFingerprint* print_right = print(right);
        const bool have = print_left != nullptr && print_right != nullptr;
        // A sampled word one side accepts and the other rejects settles that
        // direction as false, so the solver is asked only where no word
        // separated them. A timed-out query is read as "does not imply" below,
        // so a refutation can only replace an undecided verdict with the answer
        // that verdict was already being given.
        const bool left_refuted =
            have && fingerprint::refutes_implication(*print_left, *print_right);
        const bool right_refuted =
            have && fingerprint::refutes_implication(*print_right, *print_left);
        m_counters.m_refuted.fetch_add(
            static_cast<std::size_t>(left_refuted) +
                static_cast<std::size_t>(right_refuted),
            std::memory_order_relaxed);
        Verdict verdict;
        if (!left_refuted) {
            m_counters.m_solver_queries.fetch_add(1, std::memory_order_relaxed);
            verdict.m_forward =
                tlsf_spec_implies(spec(left), spec(right), m_checker)
                    .value_or(false);
        }
        if (!right_refuted) {
            m_counters.m_solver_queries.fetch_add(1, std::memory_order_relaxed);
            verdict.m_backward =
                tlsf_spec_implies(spec(right), spec(left), m_checker)
                    .value_or(false);
        }
        return verdict;
    }

    Relation settle(Relation relation, std::size_t candidate,
                    std::size_t member) {
        if (relation != Relation::Equivalent) {
            return relation;
        }
        ImplicationFilterStats::n_equivalent_collapsed.fetch_add(
            1, std::memory_order_relaxed);
        return tlsf_prefer_in_class(spec(candidate), spec(member),
                                    m_keys[candidate], m_keys[member])
                   ? Relation::MemberRemoved
                   : Relation::CandidateDropped;
    }

    // Stops at the first member that dominates the candidate, which is sound
    // because the snapshot is an antichain: if member `a` dominates the
    // candidate and the candidate dominates some other member `a'`, then a
    // dominates a', which two members of an antichain cannot do. A candidate on
    // its way out can never also have been about to remove something, so the
    // rest of the scan cannot matter. The equivalence case is the same
    // argument -- a candidate equivalent to `a` relates to every other member
    // exactly as `a` does, and `a` is incomparable to all of them.
    ScanResult scan_against(std::size_t candidate) {
        ScanResult out;
        for (std::size_t pos = 0; pos < m_snapshot.size(); ++pos) {
            const std::size_t member = m_snapshot[pos];
            const Relation relation = settle(
                relation_of(compare(candidate, member)), candidate, member);
            if (relation == Relation::CandidateDropped) {
                out.m_dropped_by = pos;
                if (pos + 1 < m_snapshot.size()) {
                    m_counters.m_short_circuited.fetch_add(
                        1, std::memory_order_relaxed);
                }
                return out;
            }
            if (relation == Relation::MemberRemoved) {
                out.m_removes.push_back(pos);
            }
        }
        return out;
    }

    void scan_wave(std::size_t count) {
        m_scans.assign(count, {});
        run_bounded_async(
            count, dispatch_window(),
            [this](std::size_t offset) {
                return [this, offset] {
                    m_scans[offset] = scan_against(m_begin + offset);
                };
            },
            [](std::size_t) {});
        m_survivors.clear();
        m_survivor_position.assign(count, k_none);
        for (std::size_t offset = 0; offset < count; ++offset) {
            if (m_scans[offset].m_dropped_by == k_none) {
                m_survivor_position[offset] = m_survivors.size();
                m_survivors.push_back(offset);
            }
        }
    }

    // Eager, losing the scan's short circuit, which is bounded by the wave size
    // squared and mostly answered by the prefilter for free. Early in a walk
    // the antichain is small, so most of a wave survives and this is the whole
    // cost.
    void reconcile() {
        const std::size_t width = m_survivors.size();
        m_matrix.assign(width * width, {});
        std::vector<std::pair<std::size_t, std::size_t>> pairs;
        pairs.reserve(width * (width > 0 ? width - 1 : 0) / 2);
        for (std::size_t i = 0; i < width; ++i) {
            for (std::size_t j = i + 1; j < width; ++j) {
                pairs.emplace_back(i, j);
            }
        }
        m_counters.m_reconciled.fetch_add(pairs.size(),
                                          std::memory_order_relaxed);
        run_bounded_async(
            pairs.size(), dispatch_window(),
            [this, &pairs, width](std::size_t index) {
                return [this, &pairs, width, index] {
                    const std::size_t left = pairs[index].first;
                    const std::size_t right = pairs[index].second;
                    m_matrix[(left * width) + right] =
                        compare(m_begin + m_survivors[left],
                                m_begin + m_survivors[right]);
                };
            },
            [](std::size_t) {});
    }

    // The matrix stores the lower survivor position as side a, and the replay
    // always reaches the earlier survivor first, so a peer that came first is
    // side a and the candidate is side b.
    [[nodiscard]] Relation peer_relation(std::size_t offset,
                                         std::size_t peer_offset) {
        const std::size_t width = m_survivors.size();
        const std::size_t self = m_survivor_position[offset];
        const std::size_t peer = m_survivor_position[peer_offset];
        const Verdict stored = peer < self ? m_matrix[(peer * width) + self]
                                           : m_matrix[(self * width) + peer];
        const Verdict oriented =
            peer < self ? Verdict{stored.m_backward, stored.m_forward} : stored;
        return settle(relation_of(oriented), m_begin + offset,
                      m_begin + peer_offset);
    }

    void emit(AntichainRow row) {
        if (m_on_row) {
            m_on_row(row);
        }
        m_rows.push_back(std::move(row));
    }

    void drop(std::size_t offset) {
        const Arrival& arrival = m_arrivals[m_begin + offset];
        emit({arrival.m_elapsed_s, arrival.m_name, AntichainEvent::Drop,
              m_antichain.size()});
    }

    void admit(std::size_t offset, const std::vector<std::size_t>& removes) {
        const std::size_t index = m_begin + offset;
        const Arrival& arrival = m_arrivals[index];
        for (const std::size_t victim : removes) {
            if (m_present[victim] == 0) {
                continue;
            }
            m_present[victim] = 0;
            m_antichain.erase(
                std::find(m_antichain.begin(), m_antichain.end(), victim));
            emit({arrival.m_elapsed_s, m_arrivals[victim].m_name,
                  AntichainEvent::Remove, m_antichain.size()});
        }
        m_antichain.push_back(index);
        m_present[index] = 1;
        m_admitted.push_back(offset);
        emit({arrival.m_elapsed_s, arrival.m_name, AntichainEvent::Admit,
              m_antichain.size()});
    }

    // A peer that has since been removed is skipped rather than compared: it
    // was removed by a member still present, and this candidate meets that
    // member in the same loop, so transitivity carries whatever verdict the
    // removed peer would have given.
    void replay_one(std::size_t offset) {
        if (m_scans[offset].m_dropped_by != k_none) {
            drop(offset);
            return;
        }
        std::vector<std::size_t> removes;
        for (const std::size_t peer_offset : m_admitted) {
            if (m_present[m_begin + peer_offset] == 0) {
                continue;
            }
            const Relation relation = peer_relation(offset, peer_offset);
            if (relation == Relation::CandidateDropped) {
                drop(offset);
                return;
            }
            if (relation == Relation::MemberRemoved) {
                removes.push_back(m_begin + peer_offset);
            }
        }
        for (const std::size_t pos : m_scans[offset].m_removes) {
            removes.push_back(m_snapshot[pos]);
        }
        admit(offset, removes);
    }

    void replay(std::size_t count) {
        m_admitted.clear();
        for (std::size_t offset = 0; offset < count; ++offset) {
            replay_one(offset);
        }
    }

    const std::vector<Arrival>& m_arrivals;
    std::vector<fingerprint::PackedFingerprint> m_prints;
    std::vector<double> m_keys;
    SatisfiabilityChecker& m_checker;
    AntichainRowCallback m_on_row;
    Counters m_counters;

    std::vector<AntichainRow> m_rows;
    std::vector<std::size_t> m_antichain;
    std::vector<char> m_present;

    std::size_t m_begin{0};
    std::vector<std::size_t> m_snapshot;
    std::vector<ScanResult> m_scans;
    std::vector<std::size_t> m_survivors;
    std::vector<std::size_t> m_survivor_position;
    std::vector<Verdict> m_matrix;
    std::vector<std::size_t> m_admitted;
};

std::vector<fingerprint::PackedFingerprint> prints_of(
    const std::vector<Arrival>& arrivals) {
    std::vector<Specification> specs;
    specs.reserve(arrivals.size());
    for (const Arrival& arrival : arrivals) {
        specs.push_back(arrival.m_specification);
    }
    return prefilter::fingerprints_of(specs);
}

}  // namespace

std::vector<AntichainRow> running_antichain(
    const std::vector<Arrival>& arrivals, SatisfiabilityChecker& checker,
    const TlsfSimilarityKey& similarity, std::size_t wave_size,
    const GenerationProgressCallback& on_progress,
    const AntichainRowCallback& on_row, AntichainStats* stats) {
    if (arrivals.empty()) {
        return {};
    }
    Walk walk(arrivals, prints_of(arrivals), similarity, checker, on_row);
    const std::size_t wave = wave_size > 0 ? wave_size : dispatch_window();
    for (std::size_t begin = 0; begin < arrivals.size(); begin += wave) {
        const std::size_t end = std::min(begin + wave, arrivals.size());
        walk.run_wave(begin, end);
        if (on_progress) {
            on_progress(end, arrivals.size());
        }
    }
    const Counters& counters = walk.counters();
    const std::size_t refuted =
        counters.m_refuted.load(std::memory_order_relaxed);
    if (stats != nullptr) {
        stats->m_solver_queries =
            counters.m_solver_queries.load(std::memory_order_relaxed);
        stats->m_refuted_directions = refuted;
        stats->m_short_circuited =
            counters.m_short_circuited.load(std::memory_order_relaxed);
        stats->m_reconciled_pairs =
            counters.m_reconciled.load(std::memory_order_relaxed);
    }
    ImplicationFilterStats::n_fingerprint_refuted.fetch_add(
        refuted, std::memory_order_relaxed);
    return walk.take_rows();
}

std::vector<std::string> antichain_members_at(
    const std::vector<AntichainRow>& rows, double elapsed_s) {
    std::vector<std::string> members;
    for (const AntichainRow& row : rows) {
        if (row.m_elapsed_s > elapsed_s) {
            break;
        }
        if (row.m_event == AntichainEvent::Admit) {
            members.push_back(row.m_name);
        } else if (row.m_event == AntichainEvent::Remove) {
            const auto found =
                std::find(members.begin(), members.end(), row.m_name);
            if (found != members.end()) {
                members.erase(found);
            }
        }
    }
    return members;
}

}  // namespace tlsf
