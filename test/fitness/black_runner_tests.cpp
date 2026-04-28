#include "fitness/black_runner.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

void test_satisfiable_simple() {
    expect(check_satisfiability("F p"),
           "black-runner: F p should be satisfiable");
}

void test_unsatisfiable_contradiction() {
    expect(!check_satisfiability("p & !p"),
           "black-runner: p & !p should be unsatisfiable");
}

void test_satisfiable_ltl() {
    expect(check_satisfiability("G F p"),
           "black-runner: G F p should be satisfiable");
}

void test_unsatisfiable_ltl() {
    expect(!check_satisfiability("G !p & F p"),
           "black-runner: G !p & F p should be unsatisfiable");
}

}  // namespace

void run_black_runner_tests() {
    test_satisfiable_simple();
    test_unsatisfiable_contradiction();
    test_satisfiable_ltl();
    test_unsatisfiable_ltl();
}
