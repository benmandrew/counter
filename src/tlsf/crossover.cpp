#include "tlsf/crossover.hpp"

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

Formula replace_everywhere(const Formula& subject, const Formula& pattern,
                           const Formula& replacement) {
    return subject.rewrite_post_order(
        [&pattern,
         &replacement](const Formula& subtree) -> std::optional<Formula> {
            if (subtree == pattern) {
                return replacement;
            }
            return std::nullopt;
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
    return replace_everywhere(into, sites[site], donors[donor]);
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
        return replace_everywhere(
            into, site_formula,
            Formula::make_binary(kind, site_formula, donor_formula));
    }
    const Formula::Kind kind =
        join_op == 2 ? Formula::Kind::Until : Formula::Kind::WeakUntil;
    const bool donor_on_the_right = random_source.next_bool();
    const Formula& lhs = donor_on_the_right ? site_formula : donor_formula;
    const Formula& rhs = donor_on_the_right ? donor_formula : site_formula;
    return replace_everywhere(into, site_formula,
                              Formula::make_binary(kind, lhs, rhs));
}

// A conjunct crossover may rewrite: its section and its slot in it. Deleted
// conjuncts are left out on both sides — a deleted conjunct is content its
// parent has thrown away, so crossover neither breeds from it nor overwrites
// it, and can therefore neither resurrect one nor delete a live one.
using Slot = std::pair<Section*, std::size_t>;

std::vector<Slot> live_slots(const std::array<Section*, 3>& sections) {
    std::vector<Slot> slots;
    for (Section* section : sections) {
        for (std::size_t i = 0; i < section->size(); ++i) {
            if (!(*section)[i].m_removed) {
                slots.emplace_back(section, i);
            }
        }
    }
    return slots;
}

std::vector<Formula> live_donors(
    const std::array<const Section*, 3>& sections) {
    std::vector<Formula> donors;
    for (const Section* section : sections) {
        for (const tlsf::SectionEntry& entry : *section) {
            if (!entry.m_removed) {
                donors.push_back(entry.m_formula);
            }
        }
    }
    return donors;
}

// One merge per side, as AuRUS: draw a conjunct of the result (which starts as
// parent A) and a conjunct of parent B, independently and from anywhere on the
// side, and graft the second into the first.
void cross_side(const std::vector<Slot>& targets,
                const std::vector<Formula>& donors,
                const RandomSource& random_source) {
    if (targets.empty() || donors.empty()) {
        return;
    }
    const std::size_t target = random_source.next_index(targets.size());
    const std::size_t donor = random_source.next_index(donors.size());
    Formula& into = (*targets[target].first)[targets[target].second].m_formula;
    if (random_source.next_bool()) {
        into = replace_subformula(into, donors[donor], random_source);
    } else {
        into = combine_subformula(into, donors[donor], random_source);
    }
}

}  // namespace

tlsf::Specification tlsf_crossover(const tlsf::Specification& parent_a,
                                   const tlsf::Specification& parent_b,
                                   const RandomSource& random_source) {
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
    cross_side(live_slots(tlsf::mutable_assumption_sections_of(result)),
               live_donors(tlsf::assumption_sections_of(parent_b)),
               random_source);
    cross_side(live_slots(tlsf::mutable_guarantee_sections_of(result)),
               live_donors(tlsf::guarantee_sections_of(parent_b)),
               random_source);
    return result;
}
