#include "filter/well_separation.hpp"

#include <string>
#include <unordered_set>
#include <utility>

#include "prop_formula.hpp"

namespace {

// Collects the atom names appearing in a propositional formula. Conditions and
// responses are guaranteed propositional (the temporal structure lives in the
// timing), so only Atom/Not/binary kinds are reachable; the temporal kinds are
// walked defensively for completeness.
void collect_atoms(const Formula& formula,
                   std::unordered_set<std::string>& out) {
    switch (formula.kind()) {
        case Formula::Kind::Atom:
            if (const std::optional<std::string> name = formula.atom_name()) {
                out.insert(*name);
            }
            return;
        case Formula::Kind::Not:
        case Formula::Kind::Next:
        case Formula::Kind::Eventually:
        case Formula::Kind::Globally:
            if (const std::optional<Formula> child = formula.unary_child()) {
                collect_atoms(*child, out);
            }
            return;
        case Formula::Kind::And:
        case Formula::Kind::Or:
        case Formula::Kind::Implies:
        case Formula::Kind::Iff:
        case Formula::Kind::Until:
        case Formula::Kind::Release:
        case Formula::Kind::WeakUntil:
            if (const std::optional<std::pair<Formula, Formula>> children =
                    formula.binary_children()) {
                collect_atoms(children->first, out);
                collect_atoms(children->second, out);
            }
            return;
    }
}

// True if any assumption's condition or response references an output atom.
// Only then can the system possibly force the assumptions to fail, so only then
// is the ltlsynt query worth running.
bool assumptions_reference_output(const Specification& specification) {
    const std::unordered_set<std::string> outputs(
        specification.m_out_atoms.begin(), specification.m_out_atoms.end());
    for (const Requirement& req : specification.m_assumptions) {
        std::unordered_set<std::string> atoms;
        collect_atoms(req.m_condition, atoms);
        collect_atoms(req.m_response, atoms);
        for (const std::string& atom : atoms) {
            if (outputs.count(atom) != 0) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

bool specification_is_not_well_separated(const Specification& specification,
                                         RealizabilityChecker& checker) {
    if (specification.m_assumptions.empty()) {
        return false;
    }
    // An assumption constraining only input atoms is well-separated by
    // construction: the system controls no atom in it and so cannot force it to
    // fail. Only when an assumption references an output atom is the ltlsynt
    // query worth the cost. Joint unsatisfiability of the assumptions is the
    // vacuity filter's concern (it runs first), not this one.
    if (!assumptions_reference_output(specification)) {
        return false;
    }
    std::string conjunction;
    for (const Requirement& req : specification.m_assumptions) {
        if (!conjunction.empty()) {
            conjunction += " & ";
        }
        conjunction += "(" + req.m_ltl + ")";
    }
    // Guarantees replaced with false: the spec becomes (assumptions) -> false,
    // i.e. !(assumptions). It is realizable exactly when the system has a
    // strategy that forces the assumptions to fail against every environment --
    // the definition of not being well-separated. The input/output partition is
    // the original spec's, so assumptions over input atoms alone stay
    // unrealizable (the system controls nothing it could use to break them).
    const std::string formula = "(" + conjunction + ") -> (false)";
    return checker.check_realizability_ltl(formula, specification.m_in_atoms,
                                           specification.m_out_atoms);
}

FilterFunction make_well_separation_filter(RealizabilityChecker& checker) {
    return make_predicate_filter(
        "not-well-separated", [&checker](const Specification& spec) {
            return !specification_is_not_well_separated(spec, checker);
        });
}
