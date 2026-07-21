#include <string>
#include <utility>
#include <vector>

#include "filter/vacuity.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

Requirement continual(const std::string& response, const Timing& tim) {
    return Requirement(Formula("true"), Formula(response), tim);
}

Specification with_assumptions(std::vector<Requirement> assumptions) {
    return Specification(std::move(assumptions),
                         {continual("grant", timing::immediately())}, {"req"},
                         {"grant"});
}

void test_no_assumptions_is_not_vacuous() {
    SatisfiabilityChecker checker;
    const Specification spec = with_assumptions({});
    expect(!specification_has_unsatisfiable_assumptions(spec, checker),
           "vacuity: a spec with no assumptions is not vacuous");
}

void test_satisfiable_assumption_is_not_vacuous() {
    SatisfiabilityChecker checker;
    const Specification spec =
        with_assumptions({continual("req", timing::within_ticks(6))});
    expect(!specification_has_unsatisfiable_assumptions(spec, checker),
           "vacuity: 'within 6 ticks' is satisfiable and not vacuous");
}

// G(!R & X(!R & ... & X R)) asserts R at tick n+1 relative to every timepoint,
// and !R at that same tick relative to the timepoint n+1 later. This is the
// candidate the strengthening direction reaches via within n -> after n-1.
void test_after_ticks_under_continual_true_is_vacuous() {
    SatisfiabilityChecker checker;
    const Specification spec =
        with_assumptions({continual("req", timing::after_ticks(5))});
    expect(specification_has_unsatisfiable_assumptions(spec, checker),
           "vacuity: 'after 5 ticks' under a continual true condition is "
           "unsatisfiable");
}

// Each assumption is satisfiable alone; only their conjunction contradicts.
void test_jointly_unsatisfiable_assumptions_are_vacuous() {
    SatisfiabilityChecker checker;
    const Specification spec =
        with_assumptions({continual("req", timing::always()),
                          continual("!req", timing::always())});
    expect(specification_has_unsatisfiable_assumptions(spec, checker),
           "vacuity: 'G req' and 'G !req' are jointly unsatisfiable");
}

void test_filter_drops_only_the_vacuous_spec() {
    SatisfiabilityChecker checker;
    FilterFunction filter = make_vacuity_filter(checker);
    const Specification good =
        with_assumptions({continual("req", timing::within_ticks(6))});
    const Specification vacuous =
        with_assumptions({continual("req", timing::after_ticks(5))});
    const auto survivors = filter({good, vacuous});
    expect(survivors.size() == 1,
           "vacuity filter: exactly one of the two specs should survive");
    expect(!specification_has_unsatisfiable_assumptions(survivors[0], checker),
           "vacuity filter: the survivor should be the satisfiable spec");
}

}  // namespace

void run_vacuity_filter_tests() {
    test_no_assumptions_is_not_vacuous();
    test_satisfiable_assumption_is_not_vacuous();
    test_after_ticks_under_continual_true_is_vacuous();
    test_jointly_unsatisfiable_assumptions_are_vacuous();
    test_filter_drops_only_the_vacuous_spec();
}
