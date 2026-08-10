#include "filter/correctness.hpp"

#include <string>
#include <vector>

#include "filter/vacuity.hpp"
#include "filter/well_separation.hpp"

std::string input_screen_warning(const std::string& check_name) {
    return "warning: the input specification fails the " + check_name +
           " check.\n"
           "  It cannot be written as a repair of itself, so a repair of this "
           "run\n"
           "  is a descendant that fixes the property as well as the "
           "unrealizability.\n";
}

std::vector<CorrectnessCheck> correctness_checks(SatisfiabilityChecker& sat,
                                                 RealizabilityChecker& real) {
    std::vector<CorrectnessCheck> checks;
    checks.push_back({"vacuity",
                      [&sat](const Specification& spec) {
                          return !specification_is_vacuous(spec, sat);
                      },
                      &Config::run_vacuity_filter});
    checks.push_back({"not-well-separated",
                      [&real](const Specification& spec) {
                          return !specification_is_not_well_separated(spec,
                                                                      real);
                      },
                      &Config::run_well_separation_filter});
    return checks;
}
