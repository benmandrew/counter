#include <string>
#include <vector>

#include "requirement.hpp"
#include "runner/formaliser.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

// A malformed requirement makes the CLI's parser write its "Line N: ..."
// message to stderr (not stdout) and still emit an empty line on stdout for
// that request, so PersistentProcess::request never throws on bad input —
// the one-line-in/one-line-out pairing holds, but a rejected line comes back
// as "". Real formalised output is never empty, so that's how a rejection
// is distinguished from a successfully formalised requirement.
void expect_valid_fretish(RequirementFormaliser& formaliser,
                          const Requirement& requirement) {
    const std::string fretish = requirement.to_string();
    const std::string result = formaliser.formalise(fretish);
    expect(
        !result.empty(),
        "Requirement::to_string: formaliser CLI rejected \"" + fretish + "\"");
}

std::string ltl_continual(const Formula& condition, const Formula& response,
                          const Timing& timing) {
    return requirement_to_ltl(
        Requirement(condition, response, timing, ConditionType::Continual));
}

std::string ltl_trigger(const Formula& condition, const Formula& response,
                        const Timing& timing) {
    return requirement_to_ltl(
        Requirement(condition, response, timing, ConditionType::Trigger));
}

// --- Continual semantics (default) ---

void test_immediately() {
    const std::string result =
        ltl_continual(Formula("t"), Formula("r"), timing::immediately());
    expect(result == "G((t) -> (r))",
           "requirement_to_ltl: Immediately should produce G(T -> R)");
}

void test_next_timepoint() {
    const std::string result =
        ltl_continual(Formula("t"), Formula("r"), timing::next_timepoint());
    expect(result == "G((t) -> X(r))",
           "requirement_to_ltl: NextTimepoint should produce G(T -> X R)");
}

void test_eventually() {
    const std::string result =
        ltl_continual(Formula("t"), Formula("r"), timing::eventually());
    expect(result == "G((t) -> F(r))",
           "requirement_to_ltl: Eventually should produce G(T -> F R)");
}

void test_always() {
    const std::string result =
        ltl_continual(Formula("t"), Formula("r"), timing::always());
    expect(result == "G((t) -> G(r))",
           "requirement_to_ltl: Always should produce G(T -> G R)");
}

void test_within_ticks_zero() {
    const std::string result =
        ltl_continual(Formula("t"), Formula("r"), timing::within_ticks(0));
    expect(result == "G((t) -> ((r)))",
           "requirement_to_ltl: WithinTicks(0) should produce G(T -> (R))");
}

void test_within_ticks_one() {
    const std::string result =
        ltl_continual(Formula("t"), Formula("r"), timing::within_ticks(1));
    expect(result == "G((t) -> ((r) | X((r))))",
           "requirement_to_ltl: WithinTicks(1) should expand to R | X(R)");
}

void test_within_ticks_two() {
    const std::string result =
        ltl_continual(Formula("t"), Formula("r"), timing::within_ticks(2));
    expect(
        result == "G((t) -> ((r) | X((r) | X((r)))))",
        "requirement_to_ltl: WithinTicks(2) should expand to R | X(R | X(R))");
}

void test_for_ticks_zero() {
    const std::string result =
        ltl_continual(Formula("t"), Formula("r"), timing::for_ticks(0));
    expect(result == "G((t) -> ((r)))",
           "requirement_to_ltl: ForTicks(0) should produce G(T -> (R))");
}

void test_for_ticks_one() {
    const std::string result =
        ltl_continual(Formula("t"), Formula("r"), timing::for_ticks(1));
    expect(result == "G((t) -> ((r) & X((r))))",
           "requirement_to_ltl: ForTicks(1) should expand to R & X(R)");
}

void test_for_ticks_two() {
    const std::string result =
        ltl_continual(Formula("t"), Formula("r"), timing::for_ticks(2));
    expect(result == "G((t) -> ((r) & X((r) & X((r)))))",
           "requirement_to_ltl: ForTicks(2) should expand to R & X(R & X(R))");
}

void test_after_ticks_zero() {
    const std::string result =
        ltl_continual(Formula("t"), Formula("r"), timing::after_ticks(0));
    expect(result == "G((t) -> (!(r) & X((r))))",
           "requirement_to_ltl: AfterTicks(0) should produce G(T -> (!R & "
           "X(R))), i.e. R must not hold at the condition tick but must hold "
           "at the next one");
}

void test_after_ticks_one() {
    const std::string result =
        ltl_continual(Formula("t"), Formula("r"), timing::after_ticks(1));
    expect(
        result == "G((t) -> (!(r) & X(!(r) & X((r)))))",
        "requirement_to_ltl: AfterTicks(1) should expand to !R & X(!R & X(R))");
}

void test_after_ticks_two() {
    const std::string result =
        ltl_continual(Formula("t"), Formula("r"), timing::after_ticks(2));
    expect(
        result == "G((t) -> (!(r) & X(!(r) & X(!(r) & X((r))))))",
        "requirement_to_ltl: AfterTicks(2) should expand to !R & X(!R & X(!R & "
        "X(R)))");
}

// --- Trigger semantics ---

void test_trigger_immediately() {
    const std::string result =
        ltl_trigger(Formula("c"), Formula("r"), timing::immediately());
    expect(result == "G((!(c) & X(c)) -> X((r))) & ((c) -> (r))",
           "requirement_to_ltl trigger: Immediately should produce "
           "G((!C & XC) -> X(R)) & (C -> R)");
}

void test_trigger_next_timepoint() {
    const std::string result =
        ltl_trigger(Formula("c"), Formula("r"), timing::next_timepoint());
    expect(result == "G((!(c) & X(c)) -> X(X(r))) & ((c) -> X(r))",
           "requirement_to_ltl trigger: NextTimepoint should produce "
           "G((!C & XC) -> X(X(R))) & (C -> X(R))");
}

void test_trigger_eventually() {
    const std::string result =
        ltl_trigger(Formula("c"), Formula("r"), timing::eventually());
    expect(result == "G((!(c) & X(c)) -> X(F(r))) & ((c) -> F(r))",
           "requirement_to_ltl trigger: Eventually should produce "
           "G((!C & XC) -> X(F(R))) & (C -> F(R))");
}

void test_trigger_always() {
    const std::string result =
        ltl_trigger(Formula("c"), Formula("r"), timing::always());
    expect(result == "G((!(c) & X(c)) -> X(G(r))) & ((c) -> G(r))",
           "requirement_to_ltl trigger: Always should produce "
           "G((!C & XC) -> X(G(R))) & (C -> G(R))");
}

void test_trigger_for_ticks_one() {
    const std::string result =
        ltl_trigger(Formula("c"), Formula("r"), timing::for_ticks(1));
    expect(result ==
               "G((!(c) & X(c)) -> X(((r) & X((r))))) & "
               "((c) -> ((r) & X((r))))",
           "requirement_to_ltl trigger: ForTicks(1) should produce "
           "G((!C & XC) -> X((R & X(R)))) & (C -> (R & X(R)))");
}

void test_trigger_within_ticks_one() {
    const std::string result =
        ltl_trigger(Formula("c"), Formula("r"), timing::within_ticks(1));
    expect(result ==
               "G((!(c) & X(c)) -> X(((r) | X((r))))) & "
               "((c) -> ((r) | X((r))))",
           "requirement_to_ltl trigger: WithinTicks(1) should produce "
           "G((!C & XC) -> X((R | X(R)))) & (C -> (R | X(R)))");
}

void test_trigger_after_ticks_one() {
    const std::string result =
        ltl_trigger(Formula("c"), Formula("r"), timing::after_ticks(1));
    expect(result ==
               "G((!(c) & X(c)) -> X((!(r) & X(!(r) & X((r)))))) & "
               "((c) -> (!(r) & X(!(r) & X((r)))))",
           "requirement_to_ltl trigger: AfterTicks(1) should produce "
           "G((!C & XC) -> X((!R & X(!R & X(R))))) & (C -> (!R & X(!R & "
           "X(R))))");
}

// --- specification_has_false_condition ---

void test_specification_has_false_condition_detects_assumption() {
    const Specification spec(
        {Requirement(Formula("false"), Formula("r"), timing::immediately())},
        {Requirement(Formula("t"), Formula("r"), timing::immediately())}, {"t"},
        {"r"});
    expect(specification_has_false_condition(spec),
           "specification_has_false_condition: should detect a false condition "
           "in an assumption");
}

void test_specification_has_false_condition_detects_guarantee() {
    const Specification spec(
        {},
        {Requirement(Formula("false"), Formula("r"), timing::immediately())},
        {"t"}, {"r"});
    expect(specification_has_false_condition(spec),
           "specification_has_false_condition: should detect a false condition "
           "in a guarantee");
}

void test_specification_has_false_condition_false_for_normal_spec() {
    const Specification spec(
        {}, {Requirement(Formula("t"), Formula("r"), timing::immediately())},
        {"t"}, {"r"});
    expect(!specification_has_false_condition(spec),
           "specification_has_false_condition: should not flag a normal "
           "condition");
}

// --- Requirement::to_string against the real FRET formaliser CLI ---

void test_to_string_is_valid_fretish_for_all_timings_and_condition_types() {
    RequirementFormaliser formaliser(formaliser_command());
    const std::vector<Timing> timings = {
        timing::immediately(),   timing::next_timepoint(),
        timing::within_ticks(3), timing::for_ticks(2),
        timing::after_ticks(1),  timing::eventually(),
        timing::always(),
    };
    for (const Timing& tim : timings) {
        expect_valid_fretish(formaliser,
                             Requirement(Formula("cond"), Formula("resp"), tim,
                                         ConditionType::Continual));
        expect_valid_fretish(formaliser,
                             Requirement(Formula("cond"), Formula("resp"), tim,
                                         ConditionType::Trigger));
    }
}

void test_to_string_is_valid_fretish_for_true_condition() {
    RequirementFormaliser formaliser(formaliser_command());
    // A literal "true" condition collapses condition_to_string() to "", so
    // this exercises the branch that omits the condition clause entirely.
    expect_valid_fretish(
        formaliser,
        Requirement(Formula::true_formula, Formula("resp"),
                    timing::immediately(), ConditionType::Continual));
    expect_valid_fretish(
        formaliser, Requirement(Formula::true_formula, Formula("resp"),
                                timing::eventually(), ConditionType::Trigger));
}

void test_to_string_is_valid_fretish_for_compound_formulae() {
    RequirementFormaliser formaliser(formaliser_command());
    expect_valid_fretish(
        formaliser,
        Requirement(Formula("a & b"), Formula("c | !d"),
                    timing::within_ticks(2), ConditionType::Continual));
}

}  // namespace

void run_requirement_tests() {
    test_immediately();
    test_next_timepoint();
    test_eventually();
    test_always();
    test_within_ticks_zero();
    test_within_ticks_one();
    test_within_ticks_two();
    test_for_ticks_zero();
    test_for_ticks_one();
    test_for_ticks_two();
    test_after_ticks_zero();
    test_after_ticks_one();
    test_after_ticks_two();
    test_trigger_immediately();
    test_trigger_next_timepoint();
    test_trigger_eventually();
    test_trigger_always();
    test_trigger_for_ticks_one();
    test_trigger_within_ticks_one();
    test_trigger_after_ticks_one();
    test_specification_has_false_condition_detects_assumption();
    test_specification_has_false_condition_detects_guarantee();
    test_specification_has_false_condition_false_for_normal_spec();
    test_to_string_is_valid_fretish_for_all_timings_and_condition_types();
    test_to_string_is_valid_fretish_for_true_condition();
    test_to_string_is_valid_fretish_for_compound_formulae();
}
