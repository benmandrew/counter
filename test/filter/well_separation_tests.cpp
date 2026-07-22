#include <string>
#include <utility>
#include <vector>

#include "filter/well_separation.hpp"
#include "requirement.hpp"
#include "runner/spot.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

Requirement continual(const std::string& response, const Timing& tim) {
    return Requirement(Formula("true"), Formula(response), tim);
}

Requirement continual_when(const std::string& condition,
                           const std::string& response, const Timing& tim) {
    return Requirement(Formula(condition), Formula(response), tim);
}

// Inputs are environment-controlled, outputs system-controlled. The system can
// force an assumption to fail only when it constrains an output atom.
Specification with_assumptions(std::vector<Requirement> assumptions) {
    return Specification(std::move(assumptions),
                         {continual("grant", timing::immediately())}, {"req"},
                         {"grant"});
}

void test_no_assumptions_is_well_separated() {
    RealizabilityChecker checker;
    const Specification spec = with_assumptions({});
    expect(!specification_is_not_well_separated(spec, checker),
           "well-separation: a spec with no assumptions is well-separated");
}

// `G req` over the input atom req: the environment can keep req true forever,
// so the system cannot force it to fail. `(G req) -> false` is unrealizable.
void test_assumption_over_input_is_well_separated() {
    RealizabilityChecker checker;
    const Specification spec =
        with_assumptions({continual("req", timing::always())});
    expect(!specification_is_not_well_separated(spec, checker),
           "well-separation: an assumption over an input atom cannot be forced "
           "to fail by the system");
}

// `G grant` over the output atom grant: the system controls grant, so it can
// simply never assert it. `(G grant) -> false` is realizable, so the spec is
// vacuously satisfiable and not well-separated.
void test_assumption_over_output_is_not_well_separated() {
    RealizabilityChecker checker;
    const Specification spec =
        with_assumptions({continual("grant", timing::always())});
    expect(specification_is_not_well_separated(spec, checker),
           "well-separation: an assumption over an output atom the system can "
           "force to fail is not well-separated");
}

// Two assumptions, `G req` over the input and `G grant` over the output. Their
// conjunction is not well-separated because the system can force `G grant` to
// fail on its own. Exercises the multi-assumption conjunction path the single-
// assumption cases never reach.
void test_conjunction_with_a_forcible_conjunct_is_not_well_separated() {
    RealizabilityChecker checker;
    const Specification spec =
        with_assumptions({continual("req", timing::always()),
                          continual("grant", timing::always())});
    expect(specification_is_not_well_separated(spec, checker),
           "well-separation: a conjunction is not well-separated when the "
           "system can force any one conjunct to fail");
}

// `G(req -> grant)` mentions the output atom grant, yet the system cannot force
// it to fail: the negation `F(req & !grant)` requires req, which the
// environment controls and can hold false forever. Well-separation is a game
// property, not atom membership -- an assumption referencing an output atom is
// not automatically droppable.
void test_output_atom_the_system_cannot_force_is_well_separated() {
    RealizabilityChecker checker;
    const Specification spec = with_assumptions(
        {continual_when("req", "grant", timing::immediately())});
    expect(!specification_is_not_well_separated(spec, checker),
           "well-separation: an assumption mentioning an output atom is still "
           "well-separated when the environment can keep it satisfied");
}

void test_filter_drops_only_the_non_well_separated_spec() {
    RealizabilityChecker checker;
    FilterFunction filter = make_well_separation_filter(checker);
    const Specification good =
        with_assumptions({continual("req", timing::always())});
    const Specification bad =
        with_assumptions({continual("grant", timing::always())});
    const auto survivors = filter({good, bad});
    expect(survivors.size() == 1,
           "well-separation filter: exactly one of the two specs should "
           "survive");
    expect(!specification_is_not_well_separated(survivors[0], checker),
           "well-separation filter: the survivor should be the well-separated "
           "spec");
}

}  // namespace

void run_well_separation_filter_tests() {
    test_no_assumptions_is_well_separated();
    test_assumption_over_input_is_well_separated();
    test_assumption_over_output_is_not_well_separated();
    test_conjunction_with_a_forcible_conjunct_is_not_well_separated();
    test_output_atom_the_system_cannot_force_is_well_separated();
    test_filter_drops_only_the_non_well_separated_spec();
}
