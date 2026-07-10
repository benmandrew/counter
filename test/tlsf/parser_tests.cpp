#include <stdexcept>
#include <string>

#include "prop_formula.hpp"
#include "runner/ltlfilt.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/specification.hpp"

namespace {

// Wraps a MAIN body (INPUTS/OUTPUTS/sections) in a minimal INFO+MAIN document.
std::string doc(const std::string& main_body,
                const std::string& semantics = "Mealy") {
    return "INFO { SEMANTICS: " + semantics + "; }\nMAIN {\n" + main_body +
           "\n}\n";
}

bool equiv(const std::string& lhs, const std::string& rhs) {
    return ltl_equivalent(lhs, rhs);
}

// Confirms the ltlfilt oracle is actually live (not silently short-circuiting
// to "true"); every equivalence assertion below relies on this.
void test_oracle_is_live() {
    expect(equiv("a", "a"), "oracle: a is equivalent to itself");
    expect(!equiv("a", "X(a)"), "oracle: a is not equivalent to X a");
}

void test_info_fields() {
    const tlsf::Specification spec = tlsf::parse(
        "INFO {\n"
        "  TITLE: \"Arbiter\";\n"
        "  DESCRIPTION: \"a demo\";\n"
        "  SEMANTICS: Mealy,standard;\n"
        "  TARGET: Mealy;\n"
        "  TAGS: \"x\", \"y\";\n"
        "  VERSION: \"1.0\";\n"
        "}\n"
        "MAIN { INPUTS { r; } OUTPUTS { g; } GUARANTEE { g; } }");
    expect(spec.m_title == "Arbiter", "info: title parsed");
    expect(spec.m_description == "a demo", "info: description parsed");
    expect(spec.m_semantics == tlsf::Semantics::MealyStandard,
           "info: semantics parsed, extra keys ignored");
    expect(spec.m_inputs.size() == 1 && spec.m_inputs[0] == "r",
           "info: inputs parsed");
    expect(spec.m_outputs.size() == 1 && spec.m_outputs[0] == "g",
           "info: outputs parsed");
}

void test_semantics_variants() {
    expect(tlsf::parse(doc("GUARANTEE { g; }", "Mealy")).m_semantics ==
               tlsf::Semantics::MealyStandard,
           "semantics: bare Mealy defaults to standard");
    expect(tlsf::parse(doc("GUARANTEE { g; }", "Moore")).m_semantics ==
               tlsf::Semantics::MooreStandard,
           "semantics: bare Moore defaults to standard");
    expect(tlsf::parse(doc("GUARANTEE { g; }", "Mealy,Strict")).m_semantics ==
               tlsf::Semantics::MealyStrict,
           "semantics: Mealy,Strict");
    expect(tlsf::parse(doc("GUARANTEE { g; }", "Moore,strict")).m_semantics ==
               tlsf::Semantics::MooreStrict,
           "semantics: Moore,strict (case-insensitive mode)");
}

void test_finite_rejected() {
    bool threw = false;
    try {
        tlsf::parse(doc("GUARANTEE { g; }", "Mealy,finite"));
    } catch (const std::invalid_argument& error) {
        threw = true;
        const std::string what = error.what();
        expect(what.find("finite") != std::string::npos,
               "semantics: finite rejection mentions finite");
    }
    expect(threw, "semantics: finite semantics is rejected");
}

void test_all_sections_and_aliases() {
    const tlsf::Specification spec =
        tlsf::parse(doc("INPUTS { a; } OUTPUTS { b; }\n"
                        "INITIALLY { a; }\n"
                        "PRESET { b; }\n"
                        "REQUIRE { a; }\n"
                        "ASSUMPTIONS { a; }\n"
                        "INVARIANTS { b; }\n"
                        "GUARANTEES { b; }\n"));
    expect(spec.m_initially.size() == 1, "sections: INITIALLY");
    expect(spec.m_preset.size() == 1, "sections: PRESET");
    expect(spec.m_require.size() == 1, "sections: REQUIRE");
    expect(spec.m_assume.size() == 1, "sections: ASSUMPTIONS alias");
    expect(spec.m_assert.size() == 1, "sections: INVARIANTS alias");
    expect(spec.m_guarantee.size() == 1, "sections: GUARANTEES alias");

    const tlsf::Specification spec2 = tlsf::parse(
        doc("ASSUME { a; } ASSERT { b; } GUARANTEE { b; } INVARIANT { a; }"));
    expect(spec2.m_assume.size() == 1, "sections: ASSUME singular");
    expect(spec2.m_assert.size() == 2, "sections: ASSERT + INVARIANT merge");
    expect(spec2.m_guarantee.size() == 1, "sections: GUARANTEE singular");
}

void test_precedence_and_associativity() {
    auto first = [](const std::string& body) {
        return tlsf::parse(
                   doc("OUTPUTS { a; b; c; } GUARANTEE { " + body + "; }"))
            .m_guarantee.front()
            .to_string();
    };
    expect(first("a -> b -> c") == "(a) -> ((b) -> (c))",
           "precedence: -> is right-associative");
    expect(first("a & b | c") == "((a) & (b)) | (c)",
           "precedence: & binds tighter than |");
    expect(first("G a & b") == "(G(a)) & (b)",
           "precedence: G binds tighter than &");
    expect(first("a U b U c") == "(a) U ((b) U (c))",
           "precedence: U is right-associative");
    expect(first("a <-> b -> c") == "(a) <-> ((b) -> (c))",
           "precedence: -> binds tighter than <->");
}

void test_bounded_expansion() {
    auto first = [](const std::string& body) {
        return tlsf::parse(doc("OUTPUTS { p; } GUARANTEE { " + body + "; }"))
            .m_guarantee.front()
            .to_string();
    };
    expect(equiv(first("X[2] p"), "X X p"), "bounded: X[2] p");
    expect(equiv(first("X[0] p"), "p"), "bounded: X[0] p is p");
    expect(equiv(first("F[0..2] p"), "p | X p | X X p"), "bounded: F[0..2] p");
    expect(equiv(first("G[1..2] p"), "X p & X X p"), "bounded: G[1..2] p");

    bool threw = false;
    try {
        first("F[0..65] p");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw, "bounded: bound over 64 throws");
}

void test_comments_and_multistatement() {
    const tlsf::Specification spec =
        tlsf::parse(doc("OUTPUTS { a; b; }\n"
                        "// a line comment\n"
                        "GUARANTEE {\n"
                        "  a; /* inline */ b;\n"
                        "}\n"));
    expect(spec.m_guarantee.size() == 2,
           "comments: two statements survive comments");
}

void expect_reject(const std::string& text, const std::string& mentions,
                   const std::string& msg) {
    bool threw = false;
    try {
        tlsf::parse(text);
    } catch (const std::invalid_argument& error) {
        threw = true;
        const std::string what = error.what();
        expect(what.find(mentions) != std::string::npos,
               msg + " (message mentions '" + mentions + "')");
    }
    expect(threw, msg);
}

void test_error_cases() {
    expect_reject(
        "INFO { SEMANTICS: Mealy; }\n"
        "GLOBAL { }\n"
        "MAIN { INPUTS { } OUTPUTS { g; } GUARANTEE { g; } }",
        "GLOBAL", "reject: GLOBAL block");
    expect_reject(doc("PARAMETERS { n = 2; } GUARANTEE { g; }"), "PARAMETERS",
                  "reject: PARAMETERS section");
    expect_reject(doc("DEFINITIONS { d = g; } GUARANTEE { g; }"), "DEFINITIONS",
                  "reject: DEFINITIONS section");
    expect_reject(doc("INPUTS { bus[4]; } GUARANTEE { g; }"), "bus",
                  "reject: bus declaration");
    expect_reject(doc("INPUTS { col { red, green }; } GUARANTEE { g; }"),
                  "enumeration", "reject: enumeration declaration");
    expect_reject(doc("OUTPUTS { g; } GUARANTEE { &&[i <- 0..2] g; }"),
                  "loop aggregate", "reject: loop aggregate");
    expect_reject(doc("OUTPUTS { g; } GUARANTEE { g@0; }"), "primed/bus-access",
                  "reject: bus-access syntax");

    // Genuine syntax errors throw invalid_argument rather than crashing.
    bool threw_missing_semi = false;
    try {
        tlsf::parse(doc("OUTPUTS { g; } GUARANTEE { g }"));
    } catch (const std::invalid_argument&) {
        threw_missing_semi = true;
    }
    expect(threw_missing_semi, "reject: missing ';' is a syntax error");

    bool threw_garbage = false;
    try {
        tlsf::parse("not a tlsf file at all");
    } catch (const std::invalid_argument&) {
        threw_garbage = true;
    }
    expect(threw_garbage, "reject: garbage input throws, never crashes");
}

void test_to_ltl_standard_lowering() {
    // GR(1)-style arbiter.
    const tlsf::Specification arbiter =
        tlsf::parse(doc("INPUTS { req; } OUTPUTS { grant; }\n"
                        "ASSUME { G F req; }\n"
                        "GUARANTEE { G (req -> F grant); }"));
    expect(equiv(arbiter.to_ltl(), "(G F req) -> G(req -> F grant)"),
           "to_ltl: arbiter lowering");

    // Guarantee-only: no assumption term, result is the guarantee conjunction.
    const tlsf::Specification guar_only =
        tlsf::parse(doc("OUTPUTS { p; } GUARANTEE { G p; }"));
    expect(equiv(guar_only.to_ltl(), "G p"),
           "to_ltl: guarantee-only has no implication");

    // Initial states + G-wrapped invariants on both sides.
    const tlsf::Specification invariants =
        tlsf::parse(doc("INPUTS { a; b; } OUTPUTS { c; d; }\n"
                        "INITIALLY { !a; }\n"
                        "REQUIRE { a -> b; }\n"
                        "PRESET { d; }\n"
                        "ASSERT { c; }"));
    expect(equiv(invariants.to_ltl(), "((!a) & G(a -> b)) -> (d & G(c))"),
           "to_ltl: initially/require/preset/assert lowering");

    // Multiple verbatim terms conjoined on each side.
    const tlsf::Specification multi =
        tlsf::parse(doc("INPUTS { a; b; } OUTPUTS { c; d; }\n"
                        "ASSUME { a; b; }\n"
                        "GUARANTEE { c; d; }"));
    expect(equiv(multi.to_ltl(), "(a & b) -> (c & d)"),
           "to_ltl: multi-statement conjunction on both sides");
}

}  // namespace

void run_tlsf_parser_tests() {
    test_oracle_is_live();
    test_info_fields();
    test_semantics_variants();
    test_finite_rejected();
    test_all_sections_and_aliases();
    test_precedence_and_associativity();
    test_bounded_expansion();
    test_comments_and_multistatement();
    test_error_cases();
    test_to_ltl_standard_lowering();
}
