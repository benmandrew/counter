#include <cmath>

#include "fitness/syntactic_similarity.hpp"
#include "prop_formula.hpp"
#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

void test_syntactic_similarity_averages_component_scores() {
    const Requirement requirement{Formula("P"), Formula("Q"),
                                  timing::immediately()};
    const Requirement other_requirement{Formula("P"), Formula("P|Q"),
                                        timing::immediately()};

    const double synsim = syntactic_similarity(requirement, other_requirement);
    expect(std::fabs(synsim - (8.0 / 9.0)) < 1e-12,
           "syntactic-similarity: component averaging should produce the "
           "expected score for 'P'/'Q' versus 'P'/'P|Q'");
}

}  // namespace

void run_syntactic_similarity_tests() {
    test_syntactic_similarity_averages_component_scores();
}
