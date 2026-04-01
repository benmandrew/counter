#include <stdexcept>
#include <string>

#include "fitness/syntactic_similarity.hpp"
#include "prop_formula.hpp"
#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

void test_syntactic_similarity_not_implemented() {
    const Requirement requirement{Formula("P"), Formula("Q"),
                                  Timing::Immediately};
    const Requirement other_requirement{Formula("P"), Formula("P|Q"),
                                        Timing::Immediately};

    bool threw = false;
    try {
        (void)syntactic_similarity(requirement, other_requirement);
    } catch (const std::logic_error& exception) {
        threw = true;
        expect(std::string{exception.what()} ==
                   "syntactic_similarity metric is not implemented yet.",
               "syntactic-similarity: default overload should propagate the "
               "not-implemented error");
    }

    expect(threw,
           "syntactic-similarity: default overload should throw logic_error "
           "until implemented");
}

}  // namespace

void run_syntactic_similarity_tests() {
    test_syntactic_similarity_not_implemented();
}
