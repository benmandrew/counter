#include <string>
#include <thread>
#include <vector>

#include "runner/ltlfilt.hpp"
#include "runner/spot.hpp"
#include "runner/spot_inprocess.hpp"
#include "runner/subprocess.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

// Stands in for a formula spot_simplify declined to parse. No simplified
// formula can collide with it, so an assertion comparing against the tool's
// answer catches a decline as well as a disagreement.
const char* const k_declined = "<declined>";

std::string via_ltlfilt(const std::string& formula) {
    const SubprocessResult result =
        run_subprocess({ltlfilt_path(), "--simplify", "-f", formula});
    std::string output = result.m_output;
    while (!output.empty() && output.back() == '\n') {
        output.pop_back();
    }
    return output;
}

// The whole point of the in-process path is that it is a drop-in for the tool,
// so the property worth pinning is agreement with it rather than any particular
// simplified form.
void test_agrees_with_ltlfilt() {
    const std::vector<std::string> formulas = {"G(p -> F(q))",
                                               "p & q",
                                               "q & p",
                                               "G(F(p))",
                                               "p & !p",
                                               "p | !p",
                                               "G(false)",
                                               "G(true)",
                                               "b | G(Fe U Gc)",
                                               "Xc & G(!b & (c W b))",
                                               "a <-> F(a | GX!Fa)"};
    for (const std::string& formula : formulas) {
        const std::string in_process =
            spot_simplify(formula).value_or(k_declined);
        const std::string from_tool = via_ltlfilt(formula);
        std::string message = "spot-simplify: ";
        message.append(formula).append(" should simplify to \"");
        message.append(from_tool).append("\", got \"");
        message.append(in_process).append("\"");
        expect(in_process == from_tool, message);
    }
}

// The last three formulae above are the ones that matter here: they are
// simplified differently at level 3 (what `--simplify` selects) than under the
// library's default options, which leave the G in place. Asking for the default
// silently disagrees with the tool on about 5% of formulae, so this pins the
// level rather than trusting it.
void test_simplification_level_matches_the_flag() {
    const std::string result =
        spot_simplify("b | G(Fe U Gc)").value_or(k_declined);
    expect(result == "b | (Fe U Gc)",
           "spot-simplify: should simplify at level 3, which drops the G; the "
           "default options keep it. Got \"" +
               result + "\"");
}

void test_unparseable_formula_is_declined() {
    expect(!spot_simplify("G(").has_value(),
           "spot-simplify: an unparseable formula should return nullopt so the "
           "caller leaves it alone");
}

// libspot's contended state is process-global rather than per-simplifier, so
// the lock inside spot_simplify is the only thing making concurrent scoring
// threads safe. This would crash rather than fail if that lock went away.
void test_concurrent_calls_agree() {
    const std::string formula = "G(p -> F(q))";
    const std::string expected = spot_simplify(formula).value_or(k_declined);
    std::vector<std::string> results(8);
    std::vector<std::thread> threads;
    threads.reserve(results.size());
    for (std::string& result : results) {
        threads.emplace_back([&result, &formula] {
            result = spot_simplify(formula).value_or(k_declined);
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    for (const std::string& result : results) {
        expect(result == expected,
               "spot-simplify: concurrent calls should agree with a "
               "single-threaded one");
    }
}

// Everything the HOA reader consumes has to match the tool. The `name:` line is
// the one exception -- ltl2tgba fills it with its own simplified rendering of
// the formula, and nothing in this project reads it -- so it is dropped before
// comparing rather than quietly ignored in the assertion.
std::string strip_name_line(const std::string& hoa) {
    std::string kept;
    std::size_t start = 0;
    while (start < hoa.size()) {
        std::size_t end = hoa.find('\n', start);
        if (end == std::string::npos) {
            end = hoa.size();
        }
        const std::string line = hoa.substr(start, end - start);
        if (line.rfind("name:", 0) != 0) {
            kept.append(line).append("\n");
        }
        start = end + 1;
    }
    return kept;
}

std::string via_ltl2tgba(const std::string& formula) {
    const SubprocessResult result =
        run_subprocess({ltl2tgba_path(), "-D", "-S", "-H", "-f", formula});
    return result.m_output;
}

void test_translation_matches_ltl2tgba() {
    const std::vector<std::string> formulas = {
        "G(p -> F(q))", "p & q", "G(F(p))",  "p U q",
        "X(p) & G(q)",  "F(p)",  "G(p | !p)"};
    for (const std::string& formula : formulas) {
        const std::string in_process =
            spot_translate_for_counting(formula).m_hoa.value_or(k_declined);
        std::string message = "spot-translate: ";
        message.append(formula).append(" should match ltl2tgba -D -S -H");
        expect(strip_name_line(in_process) ==
                   strip_name_line(via_ltl2tgba(formula)),
               message);
    }
}

void test_unparseable_formula_is_declined_by_translate() {
    expect(!spot_translate_for_counting("G(").m_hoa.has_value(),
           "spot-translate: an unparseable formula should return no automaton "
           "so the caller can fall back to the exec");
}

// SPOT 2.15.1 refuses to print the universal automaton it builds for a
// tautology. That is a library defect, not a formula error, so it has to be
// reported as such rather than surfacing as a translation failure -- otherwise
// a genuinely-true formula counts against the run's scoring-failure tolerance.
void test_tautology_print_bug_is_reported() {
    // The same minimal trigger spot_tests.cpp uses for the exec path's exit 2.
    const SpotTranslation translation =
        spot_translate_for_counting("G((a & !((b & !c) -> d)) -> b)");
    expect(translation.m_tautology_print_bug,
           "spot-translate: a tautology should be reported as the "
           "prop_complete() print bug, not as a failure");
    expect(!translation.m_hoa.has_value(),
           "spot-translate: no automaton accompanies the tautology report");
}

}  // namespace

void run_spot_inprocess_tests() {
    test_agrees_with_ltlfilt();
    test_simplification_level_matches_the_flag();
    test_unparseable_formula_is_declined();
    test_concurrent_calls_agree();
    test_translation_matches_ltl2tgba();
    test_unparseable_formula_is_declined_by_translate();
    test_tautology_print_bug_is_reported();
}
