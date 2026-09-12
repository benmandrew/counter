#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "config.hpp"
#include "filter/antichain.hpp"
#include "filter/implication.hpp"
#include "filter/implication_check.hpp"
#include "filter/streaming_maximal.hpp"
#include "fingerprint/lasso.hpp"
#include "fingerprint/prefilter.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"
#include "thread_pool.hpp"

namespace {

// "G a": a holds at every timepoint.
Requirement g_req(const std::string& atom) {
    return Requirement(Formula("true"), Formula(atom), timing::immediately());
}

// "G(true -> F a)", i.e. "GF a": a holds infinitely often. Strictly weaker
// than g_req(a) (G a implies GF a, but not vice versa), used as the "weak"
// end of the dominance chain in these tests.
Requirement f_req(const std::string& atom) {
    return Requirement(Formula("true"), Formula(atom), timing::eventually());
}

Specification make_spec(std::vector<Requirement> reqs) {
    return Specification({}, std::move(reqs), {}, {});
}

// --- make_implication_filter ---

void test_single_spec_returned_unchanged() {
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto pop = filter({make_spec({g_req("a")})});
    expect(pop.size() == 1,
           "implication_filter: single spec should be returned unchanged");
}

void test_independent_specs_both_kept() {
    // G a and G b are incomparable: neither implies the other.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto pop = filter({make_spec({g_req("a")}), make_spec({g_req("b")})});
    expect(pop.size() == 2,
           "implication_filter: incomparable specs should both be retained");
}

void test_dominated_spec_removed() {
    // G a -> GF a (if a holds always, it holds infinitely often), but not
    // vice versa. So G a strictly dominates GF a, and GF a must be removed.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto spec_strong = make_spec({g_req("a")});
    const auto spec_weak = make_spec({f_req("a")});
    const auto pop = filter({spec_strong, spec_weak});
    expect(pop.size() == 1,
           "implication_filter: dominated spec should be removed");
    expect(pop[0].m_guarantees[0].m_ltl == spec_strong.m_guarantees[0].m_ltl,
           "implication_filter: the stronger spec (G a) should survive");
}

void test_equivalent_specs_collapse_to_one() {
    // Two specs with identical LTL strings imply each other. They are one
    // repair written twice, so exactly one survives.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto pop = filter({make_spec({g_req("a")}), make_spec({g_req("a")})});
    expect(pop.size() == 1,
           "implication_filter: equivalent specs should collapse to one");
}

void test_equivalence_tie_break_prefers_similar() {
    // "G(true -> a & a)" and "G(true -> a)" are logically equivalent and
    // structurally distinct, so the survivor is decided by the tie-break
    // rather than by duplicate collapsing. The original is "G(true -> a)", so
    // the second is the more similar and must be the one kept -- and it must
    // win from either input position, the tie-break being a property of the
    // specs rather than of the order the sweep happens to visit them in.
    SatisfiabilityChecker checker;
    const Specification original = make_spec({g_req("a")});
    const Config cfg;
    for (const bool similar_first : {false, true}) {
        FilterFunction filter = make_implication_filter(
            checker, syntactic_similarity_key(original, cfg));
        const Specification bulky = make_spec({g_req("a & a")});
        const Specification lean = make_spec({g_req("a")});
        const auto pop =
            similar_first ? filter({lean, bulky}) : filter({bulky, lean});
        expect(pop.size() == 1,
               "implication_filter: equivalent specs should collapse to one");
        expect(pop.size() == 1 && pop[0] == lean,
               "implication_filter: the survivor should be the spec closest to "
               "the original");
    }
}

void test_equivalence_without_key_still_collapses() {
    // With no similarity key the tie-break falls through to operator<, which
    // is what the `maximal` tool relies on: it has no original to rank
    // against, and must still not report one repair twice.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto pop =
        filter({make_spec({g_req("a & a")}), make_spec({g_req("a")})});
    expect(pop.size() == 1,
           "implication_filter: equivalent specs should collapse with no "
           "similarity key");
}

void test_chain_keeps_only_strongest() {
    // G a & G b  =>  G a  =>  GF a  (strict chain)
    // Only the spec with both G a and G b is maximal.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto spec_strong = make_spec({g_req("a"), g_req("b")});
    const auto spec_mid = make_spec({g_req("a")});
    const auto spec_weak = make_spec({f_req("a")});
    const auto pop = filter({spec_strong, spec_mid, spec_weak});
    expect(pop.size() == 1,
           "implication_filter: chain should keep only the strongest spec");
    expect(pop[0].m_guarantees.size() == 2,
           "implication_filter: surviving spec should be the one with two "
           "requirements");
}

void test_mixed_population() {
    // A (G a & G b) strictly dominates B (G a) and C (GF a).
    // A also strictly dominates D (G b): (G a & G b) & !(G b) is UNSAT, but
    // (G b) & !(G a & G b) is SAT (b always true, a not), so D does not
    // imply A. Only A survives.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto spec_a = make_spec({g_req("a"), g_req("b")});
    const auto spec_b = make_spec({g_req("a")});
    const auto spec_c = make_spec({f_req("a")});
    const auto spec_d = make_spec({g_req("b")});
    const auto pop = filter({spec_a, spec_b, spec_c, spec_d});
    expect(pop.size() == 1,
           "implication_filter: mixed population should keep only spec with "
           "both G a and G b");
    expect(pop[0].m_guarantees.size() == 2,
           "implication_filter: surviving spec should have two requirements");
}

// --- fingerprint prefilter ---

// A sampled word may refute an implication and may never confirm one, so the
// sweep's output is what the solver alone would have produced only while every
// refutation is a genuine non-implication. That is asked of the solver here
// rather than of the argument, because the FRETISH side reaches the evaluator
// through a string round trip -- `to_ltl` renders the whole specification and
// `prop_formula_internal::try_parse_formula` reads it back -- and a silent
// misparse would fingerprint a formula other than the one queried, which
// nothing downstream would catch.
void test_fretish_prefilter_refutes_only_non_implications() {
    const std::vector<std::string> ins{"a", "b"};
    const std::vector<std::string> outs{"c"};
    const std::vector<std::string> modes{"m"};
    // Scoped requirements carry the risk the unscoped ones do not: the mode is
    // an atom of the lowering that sits in neither atom list, so a signal set
    // built from those alone would leave it false at every position.
    auto scoped = [&](const Timing& when, ScopeKind kind) {
        return Requirement(Formula("a"), Formula("c"), when,
                           ConditionType::Continual, true, false,
                           Scope{kind, "m"});
    };
    const std::vector<Specification> specs{
        Specification({}, {g_req("c")}, ins, outs, modes),
        Specification({}, {f_req("c")}, ins, outs, modes),
        Specification({}, {g_req("c"), f_req("a")}, ins, outs, modes),
        Specification({g_req("a")}, {g_req("c")}, ins, outs, modes),
        Specification({}, {scoped(timing::immediately(), ScopeKind::In)}, ins,
                      outs, modes),
        Specification({}, {scoped(timing::eventually(), ScopeKind::After)}, ins,
                      outs, modes),
        Specification({}, {scoped(timing::always(), ScopeKind::OnlyBefore)},
                      ins, outs, modes),
    };
    const std::vector<fingerprint::PackedFingerprint> prints =
        fingerprint::prefilter::fingerprints_of(specs);
    expect(prints.size() == specs.size(),
           "prefilter: every FRETISH spec should have fingerprinted");

    SatisfiabilityChecker checker;
    std::size_t refuted = 0;
    for (std::size_t i = 0; i < specs.size(); ++i) {
        for (std::size_t j = 0; j < specs.size(); ++j) {
            if (i == j ||
                !fingerprint::refutes_implication(prints[i], prints[j])) {
                continue;
            }
            ++refuted;
            const std::optional<bool> implies =
                spec_implies(specs[i], specs[j], checker);
            expect(implies.has_value() && !implies.value(),
                   "prefilter: refuted a pair the solver reads as implying");
        }
    }
    // Without this the loop above passes on an empty fingerprint table, which
    // is exactly the failure it is written to catch.
    expect(refuted > 0, "prefilter: refuted nothing, so nothing was checked");
}

// --- antichain merge ---

std::vector<Specification> sorted(std::vector<Specification> specs) {
    std::sort(specs.begin(), specs.end());
    return specs;
}

// The streaming filter's correctness rests on merging batch by batch reaching
// the antichain one sweep over the union reaches. The batches are ordered so
// the merge meets both cases: a settled spec dominated by a later arrival
// (G a, displaced by G a & G b), and an arrival dominated by a settled spec
// (G b & GF a, below G a & G b).
void test_batched_merge_matches_one_sweep() {
    SatisfiabilityChecker checker;
    const auto implies = [&checker](const Specification& lhs,
                                    const Specification& rhs) {
        return spec_implies(lhs, rhs, checker).value_or(false);
    };
    const std::vector<std::vector<Specification>> batches{
        {make_spec({g_req("a")}), make_spec({f_req("a")})},
        {make_spec({g_req("b")}), make_spec({g_req("a"), g_req("b")})},
        {make_spec({g_req("b"), f_req("a")}), make_spec({g_req("c")})},
    };

    std::vector<Specification> all;
    for (const std::vector<Specification>& batch : batches) {
        all.insert(all.end(), batch.begin(), batch.end());
    }
    const std::vector<uint8_t> one_sweep_flags =
        antichain::subsumed_in(all, implies, SimilarityKey{}, {});
    std::vector<Specification> one_sweep;
    for (std::size_t idx = 0; idx < all.size(); ++idx) {
        if (one_sweep_flags[idx] == 0U) {
            one_sweep.push_back(all[idx]);
        }
    }

    std::vector<Specification> settled;
    for (const std::vector<Specification>& batch : batches) {
        std::vector<Specification> specs = settled;
        specs.insert(specs.end(), batch.begin(), batch.end());
        const std::vector<uint8_t> flags = antichain::merge_subsumed(
            specs, antichain::keys_of(specs, SimilarityKey{}),
            fingerprint::prefilter::fingerprints_of(specs), settled.size(),
            implies, TaskPriority::Background, {});
        settled.clear();
        for (std::size_t idx = 0; idx < specs.size(); ++idx) {
            if (flags[idx] == 0U) {
                settled.push_back(specs[idx]);
            }
        }
    }

    expect(one_sweep.size() == 2,
           "antichain: one sweep should keep G a & G b and G c");
    expect(sorted(settled) == sorted(one_sweep),
           "antichain: merging batch by batch should reach the antichain one "
           "sweep over their union reaches");
}

// --- StreamingMaximalFilter ---

// A directory unique to this suite, removed on scope exit.
class TempDir {
   public:
    TempDir()
        : m_path(std::filesystem::temp_directory_path() /
                 "counter_streaming_maximal_tests") {
        std::filesystem::remove_all(m_path);
        std::filesystem::create_directories(m_path);
    }
    ~TempDir() { std::filesystem::remove_all(m_path); }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;

    [[nodiscard]] std::string listing() const {
        return (m_path / "maximal.tsv").string();
    }

    [[nodiscard]] std::string curve() const {
        return (m_path / "maximal_curve.tsv").string();
    }

    [[nodiscard]] MaximalStreamOutput output(
        std::function<double()> elapsed = {}) const {
        return {listing(), curve(), std::move(elapsed)};
    }

   private:
    std::filesystem::path m_path;
};

MaximalStreamRules<Specification> fretish_rules() {
    MaximalStreamRules<Specification> rules;
    rules.implies = [](const Specification& lhs, const Specification& rhs,
                       SatisfiabilityChecker& checker) {
        return spec_implies(lhs, rhs, checker).value_or(false);
    };
    rules.fingerprints = [](const std::vector<Specification>& specs) {
        return fingerprint::prefilter::fingerprints_of(specs);
    };
    return rules;
}

std::string read_whole(const std::string& path) {
    std::ifstream file(path);
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

std::vector<std::string> read_lines(const std::string& path) {
    std::istringstream text(read_whole(path));
    std::vector<std::string> lines;
    for (std::string line; std::getline(text, line);) {
        lines.push_back(line);
    }
    return lines;
}

// Blocks until the curve holds @p rows complete data rows, so that the next
// push lands in a batch of its own. Only the coordinator's own progress is
// waited on, and the bound turns a row that never arrives into a failed
// assertion rather than a hung suite. A row still being written ends in no
// newline, so the trailing-newline test is what keeps a partial row from
// counting.
bool curve_reaches(const std::string& path, std::size_t rows) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < deadline) {
        const std::string text = read_whole(path);
        if (!text.empty() && text.back() == '\n' &&
            read_lines(path).size() >= rows + 1) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

// The batches of test_batched_merge_matches_one_sweep, pushed one at a time
// with a name each and G a pushed twice. The result has to match the batch
// filter whatever batches the coordinator happened to form, and the listing
// has to name the maximal members in push order and nothing else.
void test_streaming_matches_batch_filter() {
    const std::vector<Specification> specs{
        make_spec({g_req("a")}),
        make_spec({f_req("a")}),
        make_spec({g_req("b")}),
        make_spec({g_req("a"), g_req("b")}),
        make_spec({g_req("b"), f_req("a")}),
        make_spec({g_req("c")}),
    };
    SatisfiabilityChecker checker;
    const std::vector<Specification> batch =
        make_implication_filter(checker)(specs);

    const TempDir dir;
    const Config cfg;
    StreamingMaximalFilter<Specification> stream(cfg, fretish_rules(),
                                                 dir.output());
    for (std::size_t idx = 0; idx < specs.size(); ++idx) {
        stream.push(specs[idx], "r" + std::to_string(idx) + ".json");
    }
    stream.push(specs[0], "r6.json");
    const std::vector<Specification> streamed = stream.finish();

    expect(sorted(streamed) == sorted(batch),
           "streaming_maximal: should keep what the batch filter keeps");
    expect(streamed.size() == 2 && streamed[0] == specs[3] &&
               streamed[1] == specs[5],
           "streaming_maximal: should return the maximal set in push order");
    const MaximalStreamCounts& counts = stream.counts();
    expect(counts.n_pushed == 7 && counts.n_distinct == 6,
           "streaming_maximal: should count 7 pushed and 6 distinct");
    expect(read_whole(dir.listing()) == "file\nr3.json\nr5.json\n",
           "streaming_maximal: the listing should name r3 and r5 alone");
    expect(!std::filesystem::exists(dir.listing() + ".tmp"),
           "streaming_maximal: no temporary listing should be left behind");
    // How many batches the coordinator formed is its own business, so only the
    // last row is a function of the pushes: it reports the six distinct
    // specifications and the two maximal over them that finish() just returned.
    const std::vector<std::string> curve = read_lines(dir.curve());
    expect(curve.size() >= 2 &&
               curve.front() == "elapsed_s\tn_pushed\tn_maximal\tn_named",
           "streaming_maximal: the curve should carry a header and a row");
    expect(curve.back() == "0.000000\t6\t2\t2",
           "streaming_maximal: the last curve row should report 6 pushed and 2 "
           "maximal");
}

// A member leaves the maximal set when a later batch subsumes it, so the curve
// falls where the listing cannot show it -- the whole reason that file is
// appended to rather than rewritten. The rule is structural so that waiting for
// a batch costs no subprocess: one specification implies another when it
// carries every one of that one's guarantees.
void test_streaming_curve_records_a_drop() {
    MaximalStreamRules<Specification> rules;
    rules.implies = [](const Specification& lhs, const Specification& rhs,
                       SatisfiabilityChecker&) {
        return std::all_of(rhs.m_guarantees.begin(), rhs.m_guarantees.end(),
                           [&lhs](const Requirement& req) {
                               return std::find(lhs.m_guarantees.begin(),
                                                lhs.m_guarantees.end(),
                                                req) != lhs.m_guarantees.end();
                           });
    };

    const TempDir dir;
    const Config cfg;
    {
        // A second per row, so a stamp says which row was written when.
        StreamingMaximalFilter<Specification> stream(
            cfg, std::move(rules),
            dir.output([seconds = 0.0]() mutable { return seconds += 1.0; }));
        stream.push(make_spec({g_req("a")}), "a.json");
        expect(curve_reaches(dir.curve(), 1),
               "streaming_maximal: the first push should write a curve row");
        stream.push(make_spec({g_req("b")}), "b.json");
        expect(curve_reaches(dir.curve(), 2),
               "streaming_maximal: the second push should write a curve row");
        // Implies both of the others, so the set falls from two to one.
        stream.push(make_spec({g_req("a"), g_req("b")}), "ab.json");
        const std::vector<Specification> streamed = stream.finish();
        expect(streamed.size() == 1,
               "streaming_maximal: ab should subsume a and b");
    }
    expect(read_whole(dir.curve()) ==
               "elapsed_s\tn_pushed\tn_maximal\tn_named\n"
               "1.000000\t1\t1\t1\n"
               "2.000000\t2\t2\t2\n"
               "3.000000\t3\t1\t1\n",
           "streaming_maximal: the curve should record the fall from 2 to 1");
    expect(read_whole(dir.listing()) == "file\nab.json\n",
           "streaming_maximal: the listing should name the surviving file");
}

// A failed check must reach the caller rather than leave a smaller set that
// reads as a result.
void test_streaming_rethrows_a_failed_check() {
    MaximalStreamRules<Specification> rules;
    rules.implies = [](const Specification&, const Specification&,
                       SatisfiabilityChecker&) -> bool {
        throw std::runtime_error("check failed");
    };
    const Config cfg;
    StreamingMaximalFilter<Specification> stream(cfg, std::move(rules), {});
    stream.push(make_spec({g_req("a")}), {});
    stream.push(make_spec({f_req("a")}), {});
    bool threw = false;
    try {
        static_cast<void>(stream.finish());
    } catch (const std::runtime_error&) {
        threw = true;
    }
    expect(threw, "streaming_maximal: finish() should rethrow a failed check");
}

// Unwinding past an unfinished filter must not wait on the solver or hang.
void test_streaming_destroyed_unfinished() {
    const Config cfg;
    {
        StreamingMaximalFilter<Specification> stream(cfg, fretish_rules(), {});
        stream.push(make_spec({g_req("a")}), {});
        stream.push(make_spec({f_req("a")}), {});
    }
}

}  // namespace

// --- spec_implies propositional shortcut ---
// These tests cover the propositional-response shortcut in requirement_implies:
// when two requirements share the same condition, timing, and condition_type,
// implication reduces to a propositional check on the responses alone, avoiding
// the expensive temporal LTL check that can time out under concurrent load.

void test_weakening_response_implies() {
    // A weaker (dropped-conjunct) response on the same condition/timing
    // must be recognised as implied by the original: the propositional shortcut
    // should confirm that (!a & b) -> b without needing a temporal LTL check.
    SatisfiabilityChecker checker;
    const Specification original(
        {},
        {Requirement(Formula("true"), Formula("!a & b"),
                     timing::within_ticks(5))},
        {}, {});
    const Specification candidate(
        {},
        {Requirement(Formula("true"), Formula("b"), timing::within_ticks(5))},
        {}, {});
    expect(spec_implies(original, candidate, checker).value_or(false),
           "spec_implies: weaker response (b) implied by (!a & b)");
    expect(!spec_implies(candidate, original, checker).value_or(true),
           "spec_implies: stronger response (!a & b) not implied by (b)");
}

void test_independent_responses_not_implied() {
    // Two requirements with unrelated responses: neither implies the other.
    SatisfiabilityChecker checker;
    const Specification spec_a(
        {},
        {Requirement(Formula("true"), Formula("a"), timing::within_ticks(5))},
        {}, {});
    const Specification spec_b(
        {},
        {Requirement(Formula("true"), Formula("b"), timing::within_ticks(5))},
        {}, {});
    expect(!spec_implies(spec_a, spec_b, checker).value_or(true),
           "spec_implies: unrelated responses should not imply each other (a)");
    expect(!spec_implies(spec_b, spec_a, checker).value_or(true),
           "spec_implies: unrelated responses should not imply each other (b)");
}

void run_implication_filter_tests() {
    test_single_spec_returned_unchanged();
    test_independent_specs_both_kept();
    test_dominated_spec_removed();
    test_equivalent_specs_collapse_to_one();
    test_equivalence_tie_break_prefers_similar();
    test_equivalence_without_key_still_collapses();
    test_chain_keeps_only_strongest();
    test_mixed_population();
    test_weakening_response_implies();
    test_independent_responses_not_implied();
    test_fretish_prefilter_refutes_only_non_implications();
    test_batched_merge_matches_one_sweep();
    test_streaming_matches_batch_filter();
    test_streaming_curve_records_a_drop();
    test_streaming_rethrows_a_failed_check();
    test_streaming_destroyed_unfinished();
}
