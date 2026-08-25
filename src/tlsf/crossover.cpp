#include "tlsf/crossover.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "prop_formula.hpp"

namespace {

using tlsf::Section;

bool is_temporal(Formula::Kind kind) {
    switch (kind) {
        case Formula::Kind::Next:
        case Formula::Kind::Eventually:
        case Formula::Kind::Globally:
        case Formula::Kind::Until:
        case Formula::Kind::Release:
        case Formula::Kind::WeakUntil:
            return true;
        case Formula::Kind::Atom:
        case Formula::Kind::Not:
        case Formula::Kind::And:
        case Formula::Kind::Or:
        case Formula::Kind::Implies:
        case Formula::Kind::Iff:
            return false;
    }
    return false;
}

// The distinct subformulae of @p formula rooted at a temporal operator, in
// post-order. AuRUS draws both ends of a graft from this set — `subformulas(
// Formula.TemporalOperator.class)` — so it moves whole temporal subtrees and
// never a bare propositional fragment. It is a set rather than a list, so a
// subformula occurring twice is no likelier to be drawn than one occurring
// once; the dedup here keeps that.
void collect_temporal(const Formula& formula, std::vector<Formula>& out) {
    if (const auto child = formula.unary_child(); child.has_value()) {
        collect_temporal(*child, out);
    } else if (const auto children = formula.binary_children();
               children.has_value()) {
        collect_temporal(children->first, out);
        collect_temporal(children->second, out);
    }
    if (!is_temporal(formula.kind())) {
        return;
    }
    for (const Formula& seen : out) {
        if (seen == formula) {
            return;
        }
    }
    out.push_back(formula);
}

// Where AuRUS finds no temporal subformula it abandons the merge and drops the
// conjunct, which counter cannot do — a slot is positional and deletion is
// mutation's move alone. The whole conjunct stands in as the single graft site
// instead, which degrades the merge to the whole-conjunct swap this operator
// used to do unconditionally. Without the fallback crossover would be a no-op
// on every purely propositional conjunct, and INITIALLY, PRESET, REQUIRE and
// ASSERT are routinely propositional: their temporal operator is added by the
// lowering, not stored.
std::vector<Formula> graft_sites(const Formula& formula) {
    std::vector<Formula> sites;
    collect_temporal(formula, sites);
    if (sites.empty()) {
        sites.push_back(formula);
    }
    return sites;
}

// One occurrence gives way, not every one. A graft site is drawn as a *value*
// rather than as a position, so a conjunct holding the same subformula twice
// had both rewritten before 2026-08-19, which multiplied the donor and the
// size delta a single merge was supposed to make. Only 6 of the 25 aurus-h2h
// inputs hold a repeated temporal subformula, so this changes little in the
// corpus and makes the operator match what its callers document.
Formula replace_first(const Formula& subject, const Formula& pattern,
                      const Formula& replacement) {
    bool replaced = false;
    return subject.rewrite_post_order(
        [&pattern, &replacement,
         &replaced](const Formula& subtree) -> std::optional<Formula> {
            if (replaced || !(subtree == pattern)) {
                return std::nullopt;
            }
            replaced = true;
            return replacement;
        });
}

// AuRUS's replaceSubformula: one subformula of @p into gives way to one drawn
// from @p from.
Formula replace_subformula(const Formula& into, const Formula& from,
                           const RandomSource& random_source) {
    const std::vector<Formula> sites = graft_sites(into);
    const std::vector<Formula> donors = graft_sites(from);
    // Sequenced into locals: both draw, and argument evaluation order is
    // unspecified.
    const std::size_t site = random_source.next_index(sites.size());
    const std::size_t donor = random_source.next_index(donors.size());
    return replace_first(into, sites[site], donors[donor]);
}

// AuRUS's combineSubformula: a subformula of @p into is joined with one drawn
// from @p from under o ∈ {∧, ∨, U, W}, and the join takes its place. U and W
// are not commutative, so which side the donor lands on is a further draw;
// ∧ and ∨ take none, as in AuRUS.
Formula combine_subformula(const Formula& into, const Formula& from,
                           const RandomSource& random_source) {
    const std::vector<Formula> sites = graft_sites(into);
    const std::vector<Formula> donors = graft_sites(from);
    const std::size_t site = random_source.next_index(sites.size());
    const std::size_t donor = random_source.next_index(donors.size());
    const Formula& site_formula = sites[site];
    const Formula& donor_formula = donors[donor];
    const std::size_t join_op = random_source.next_index(4);
    if (join_op < 2) {
        const Formula::Kind kind =
            join_op == 0 ? Formula::Kind::And : Formula::Kind::Or;
        return replace_first(
            into, site_formula,
            Formula::make_binary(kind, site_formula, donor_formula));
    }
    const Formula::Kind kind =
        join_op == 2 ? Formula::Kind::Until : Formula::Kind::WeakUntil;
    const bool donor_on_the_right = random_source.next_bool();
    const Formula& lhs = donor_on_the_right ? site_formula : donor_formula;
    const Formula& rhs = donor_on_the_right ? donor_formula : site_formula;
    return replace_first(into, site_formula,
                         Formula::make_binary(kind, lhs, rhs));
}

// A conjunct crossover may rewrite: its section and its slot in it. Deleted
// conjuncts are left out on both sides — a deleted conjunct is content its
// parent has thrown away, so crossover neither breeds from it nor overwrites
// it, and can therefore neither resurrect one nor delete a live one.
struct Slot {
    Section* m_section;
    std::size_t m_index;
    std::size_t m_section_index;
};

std::vector<Slot> live_slots(const std::array<Section*, 3>& sections) {
    std::vector<Slot> slots;
    for (std::size_t index = 0; index < sections.size(); ++index) {
        Section* section = sections[index];
        for (std::size_t i = 0; i < section->size(); ++i) {
            if (!(*section)[i].m_removed) {
                slots.push_back({section, i, index});
            }
        }
    }
    return slots;
}

// Donors are collected per section rather than pooled, so a target can be
// given the donors its own section admits. Index 0 is the initial-condition
// section, INITIALLY or PRESET, which basic TLSF requires to be propositional
// over one side's own signals; a donor from REQUIRE or ASSUME satisfies
// neither constraint, and the aurus-h2h corpus carries 15 INITIALLY entries
// that acquired a temporal operator this way.
using DonorPools = std::array<std::vector<Formula>, 3>;

DonorPools live_donors(const std::array<const Section*, 3>& sections) {
    DonorPools donors;
    for (std::size_t index = 0; index < sections.size(); ++index) {
        for (const tlsf::SectionEntry& entry : *sections[index]) {
            if (!entry.m_removed) {
                donors[index].push_back(entry.m_formula);
            }
        }
    }
    return donors;
}

// Every conjunct on the side may donate, except into an initial condition,
// which takes only its counterpart section.
std::vector<Formula> donors_for(const DonorPools& pools,
                                std::size_t section_index) {
    if (section_index == 0) {
        return pools[0];
    }
    std::vector<Formula> all;
    for (const std::vector<Formula>& pool : pools) {
        all.insert(all.end(), pool.begin(), pool.end());
    }
    return all;
}

// One merge per side, as AuRUS: draw a conjunct of the result (which starts as
// parent A) and a conjunct of parent B, independently and from anywhere on the
// side, and graft the second into the first.
void cross_side(const std::vector<Slot>& targets, const DonorPools& pools,
                const RandomSource& random_source) {
    // The donor pool depends on the target's section, so the target is drawn
    // before the pool can be tested for emptiness.
    if (targets.empty()) {
        return;
    }
    const std::size_t target = random_source.next_index(targets.size());
    const Slot& slot = targets[target];
    const std::vector<Formula> donors = donors_for(pools, slot.m_section_index);
    if (donors.empty()) {
        return;
    }
    const std::size_t donor = random_source.next_index(donors.size());
    Formula& into = (*slot.m_section)[slot.m_index].m_formula;
    if (random_source.next_bool()) {
        into = replace_subformula(into, donors[donor], random_source);
    } else {
        into = combine_subformula(into, donors[donor], random_source);
    }
}

// Appends a live ASSUME conjunct of parent B that the offspring does not
// already hold, and reports whether it did.
//
// This is AuRUS's level-1 move -- its crossover unions conjunct subsets -- and
// counter's graft cannot express it. The graft draws one conjunct per side and
// writes the merge back into the slot it came from, so a whole conjunct never
// crosses and an assumption one parent has built stays with that parent. On
// the guarantee side that restriction is load-bearing: slot i of a candidate
// must keep descending from slot i of the original, which is why deletion is
// tombstoned rather than erased. The assumption side carries no such
// invariant. It already grows, by tlsf_add_assumption and by the clone, and
// tlsf_syntactic_similarity pairs on min(size, size) and divides by max, so a
// surplus conjunct scores zero rather than shifting what any other conjunct is
// compared against.
//
// It propagates rather than constructs. The ideal assumption of a family is
// reachable only if some individual has already built it, so this earns its
// place beside the width draw in tlsf_add_assumption and not instead of it.
//
// A conjunct parent A already holds is not a donor: appending it would
// duplicate rather than union, which is what the clone operator does and pays
// for separately. Only ASSUME is drawn from, for the reason the clone gives --
// an INITIALLY entry is an initial condition and a REQUIRE entry is lowered
// under an implicit G, so moving either into ASSUME changes what it says.
//
// The probability is read before the RandomSource is touched, so at 0 this
// costs no draw and the stream is what it was before it existed.
bool union_assumption(tlsf::Specification& result,
                      const tlsf::Specification& parent_b,
                      const RandomSource& random_source, const Config& cfg) {
    if (cfg.tlsf_p_union_assumption <= 0.0) {
        return false;
    }
    std::vector<std::size_t> donors;
    for (std::size_t index = 0; index < parent_b.m_assume.size(); ++index) {
        const tlsf::SectionEntry& entry = parent_b.m_assume[index];
        if (entry.m_removed) {
            continue;
        }
        const bool held = std::any_of(
            result.m_assume.begin(), result.m_assume.end(),
            [&entry](const tlsf::SectionEntry& mine) {
                return !mine.m_removed && mine.m_formula == entry.m_formula;
            });
        if (!held) {
            donors.push_back(index);
        }
    }
    if (donors.empty() ||
        random_source.next_real() >= cfg.tlsf_p_union_assumption) {
        return false;
    }
    const std::size_t choice = random_source.next_index(donors.size());
    result.m_assume.push_back(parent_b.m_assume[donors[choice]]);
    return true;
}

}  // namespace

tlsf::Specification tlsf_crossover(const tlsf::Specification& parent_a,
                                   const tlsf::Specification& parent_b,
                                   const RandomSource& random_source,
                                   const Config& cfg) {
    // Only the signals have to match. Section sizes need not: the offspring
    // keeps parent A's shape whatever parent B's is, since the merge is
    // written back into a slot of A. Requiring equal sizes, as index-for-index
    // crossover did, meant the first individual to gain an assumption could no
    // longer breed with any other.
    if (parent_a.m_inputs != parent_b.m_inputs ||
        parent_a.m_outputs != parent_b.m_outputs) {
        return parent_a;
    }
    tlsf::Specification result = parent_a;
    // The union and the graft are alternatives on this side, not a pair: each
    // is one recombination of the assumption side, and firing both would make
    // a crossover that recombines it twice where every other side gets one.
    if (!union_assumption(result, parent_b, random_source, cfg)) {
        cross_side(live_slots(tlsf::mutable_assumption_sections_of(result)),
                   live_donors(tlsf::assumption_sections_of(parent_b)),
                   random_source);
    }
    cross_side(live_slots(tlsf::mutable_guarantee_sections_of(result)),
               live_donors(tlsf::guarantee_sections_of(parent_b)),
               random_source);
    return result;
}
