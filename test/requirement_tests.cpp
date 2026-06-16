#include <string>

#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

std::string ltl(const Formula& trigger, const Formula& response,
                const Timing& timing) {
    return requirement_to_ltl(Requirement(trigger, response, timing));
}

void test_immediately() {
    const std::string result =
        ltl(Formula("t"), Formula("r"), timing::immediately());
    expect(result == "G((t) -> (r))",
           "requirement_to_ltl: Immediately should produce G(T -> R)");
}

void test_next_timepoint() {
    const std::string result =
        ltl(Formula("t"), Formula("r"), timing::next_timepoint());
    expect(result == "G((t) -> X(r))",
           "requirement_to_ltl: NextTimepoint should produce G(T -> X R)");
}

void test_eventually() {
    const std::string result =
        ltl(Formula("t"), Formula("r"), timing::eventually());
    expect(result == "G((t) -> F(r))",
           "requirement_to_ltl: Eventually should produce G(T -> F R)");
}

void test_within_ticks_zero() {
    const std::string result =
        ltl(Formula("t"), Formula("r"), timing::within_ticks(0));
    expect(result == "G((t) -> ((r)))",
           "requirement_to_ltl: WithinTicks(0) should produce G(T -> (R))");
}

void test_within_ticks_one() {
    const std::string result =
        ltl(Formula("t"), Formula("r"), timing::within_ticks(1));
    expect(result == "G((t) -> ((r) | X((r))))",
           "requirement_to_ltl: WithinTicks(1) should expand to R | X(R)");
}

void test_within_ticks_two() {
    const std::string result =
        ltl(Formula("t"), Formula("r"), timing::within_ticks(2));
    expect(
        result == "G((t) -> ((r) | X((r) | X((r)))))",
        "requirement_to_ltl: WithinTicks(2) should expand to R | X(R | X(R))");
}

void test_for_ticks_zero() {
    const std::string result =
        ltl(Formula("t"), Formula("r"), timing::for_ticks(0));
    expect(result == "G((t) -> ((r)))",
           "requirement_to_ltl: ForTicks(0) should produce G(T -> (R))");
}

void test_for_ticks_one() {
    const std::string result =
        ltl(Formula("t"), Formula("r"), timing::for_ticks(1));
    expect(result == "G((t) -> ((r) & X((r))))",
           "requirement_to_ltl: ForTicks(1) should expand to R & X(R)");
}

void test_for_ticks_two() {
    const std::string result =
        ltl(Formula("t"), Formula("r"), timing::for_ticks(2));
    expect(result == "G((t) -> ((r) & X((r) & X((r)))))",
           "requirement_to_ltl: ForTicks(2) should expand to R & X(R & X(R))");
}

void test_after_ticks_zero() {
    const std::string result =
        ltl(Formula("t"), Formula("r"), timing::after_ticks(0));
    expect(result == "G((t) -> (r))",
           "requirement_to_ltl: AfterTicks(0) should produce G(T -> R)");
}

void test_after_ticks_one() {
    const std::string result =
        ltl(Formula("t"), Formula("r"), timing::after_ticks(1));
    expect(result == "G((t) -> (!(r) & X((r))))",
           "requirement_to_ltl: AfterTicks(1) should expand to !R & X(R)");
}

void test_after_ticks_two() {
    const std::string result =
        ltl(Formula("t"), Formula("r"), timing::after_ticks(2));
    expect(
        result == "G((t) -> (!(r) & X(!(r) & X((r)))))",
        "requirement_to_ltl: AfterTicks(2) should expand to !R & X(!R & X(R))");
}

void test_specification_has_false_trigger_detects_assumption() {
    const Specification spec(
        {Requirement(Formula("false"), Formula("r"), timing::immediately())},
        {Requirement(Formula("t"), Formula("r"), timing::immediately())}, {"t"},
        {"r"});
    expect(specification_has_false_trigger(spec),
           "specification_has_false_trigger: should detect a false trigger "
           "in an assumption");
}

void test_specification_has_false_trigger_detects_guarantee() {
    const Specification spec(
        {},
        {Requirement(Formula("false"), Formula("r"), timing::immediately())},
        {"t"}, {"r"});
    expect(specification_has_false_trigger(spec),
           "specification_has_false_trigger: should detect a false trigger "
           "in a guarantee");
}

void test_specification_has_false_trigger_false_for_normal_spec() {
    const Specification spec(
        {}, {Requirement(Formula("t"), Formula("r"), timing::immediately())},
        {"t"}, {"r"});
    expect(!specification_has_false_trigger(spec),
           "specification_has_false_trigger: should not flag a normal "
           "trigger");
}

}  // namespace

void run_requirement_tests() {
    test_immediately();
    test_next_timepoint();
    test_eventually();
    test_within_ticks_zero();
    test_within_ticks_one();
    test_within_ticks_two();
    test_for_ticks_zero();
    test_for_ticks_one();
    test_for_ticks_two();
    test_after_ticks_zero();
    test_after_ticks_one();
    test_after_ticks_two();
    test_specification_has_false_trigger_detects_assumption();
    test_specification_has_false_trigger_detects_guarantee();
    test_specification_has_false_trigger_false_for_normal_spec();
}
