#include <chrono>
#include <cstddef>
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

// The lock budget the callers use, and "no deadline" -- the default, and what
// every test that is not about deadlines wants.
constexpr std::chrono::milliseconds k_budget{8};
constexpr std::chrono::milliseconds k_no_deadline{0};

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

// Uncontended, the budgeted entry point must behave exactly like the blocking
// one. The fallback it enables is only sound because the two paths agree, so a
// disagreement here would make which path a call took observable.
void test_budgeted_simplify_agrees_when_uncontended() {
    const std::string formula = "b | G(Fe U Gc)";
    const SpotSimplification budgeted = spot_try_simplify(formula, k_budget);
    expect(!budgeted.m_lock_busy,
           "spot-simplify: an uncontended lock should not report busy");
    expect(budgeted.m_formula.value_or(k_declined) ==
               spot_simplify(formula).value_or(k_declined),
           "spot-simplify: the budgeted path should agree with the blocking "
           "one");
}

void test_budgeted_simplify_declines_unparseable() {
    const SpotSimplification budgeted = spot_try_simplify("G(", k_budget);
    expect(!budgeted.m_lock_busy,
           "spot-simplify: an unparseable formula is not a busy lock");
    expect(!budgeted.m_formula.has_value(),
           "spot-simplify: an unparseable formula should yield no result");
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
            spot_translate_for_counting(formula, k_budget, k_no_deadline)
                .m_hoa.value_or(k_declined);
        std::string message = "spot-translate: ";
        message.append(formula).append(" should match ltl2tgba -D -S -H");
        expect(strip_name_line(in_process) ==
                   strip_name_line(via_ltl2tgba(formula)),
               message);
    }
}

void test_unparseable_formula_is_declined_by_translate() {
    expect(!spot_translate_for_counting("G(", k_budget, k_no_deadline)
                .m_hoa.has_value(),
           "spot-translate: an unparseable formula should return no automaton "
           "so the caller can fall back to the exec");
}

// SPOT 2.15.1 refuses to print the universal automaton it builds for a
// tautology. That is a library defect, not a formula error, so it has to be
// reported as such rather than surfacing as a translation failure -- otherwise
// a genuinely-true formula counts against the run's scoring-failure tolerance.
void test_tautology_print_bug_is_reported() {
    // The same minimal trigger spot_tests.cpp uses for the exec path's exit 2.
    const SpotTranslation translation = spot_translate_for_counting(
        "G((a & !((b & !c) -> d)) -> b)", k_budget, k_no_deadline);
    expect(translation.m_tautology_print_bug,
           "spot-translate: a tautology should be reported as the "
           "prop_complete() print bug, not as a failure");
    expect(!translation.m_hoa.has_value(),
           "spot-translate: no automaton accompanies the tautology report");
}

// A deadline moves the work onto another thread, so the question this answers
// is whether that changes any answer. It must not: which path a translation
// took is meant to be invisible, and a disagreement here would show up in a
// campaign only as counts that differ from a run configured slightly
// differently -- with nothing to point at the cause.
void test_a_deadline_does_not_change_the_answer() {
    const std::vector<std::string> formulas = {
        "G(p -> F(q))", "p & q", "G(F(p))",          "p U q",
        "X(p) & G(q)",  "F(p)",  "G((a -> b) U !c)", "!(a U (b & Xc))"};
    for (const std::string& formula : formulas) {
        const SpotTranslation unbounded =
            spot_translate_for_counting(formula, k_budget, k_no_deadline);
        const SpotTranslation deadlined = spot_translate_for_counting(
            formula, k_budget, std::chrono::seconds(60));
        expect(!deadlined.m_timed_out,
               "spot-translate: 60s is not a deadline any of these can miss");
        expect(deadlined.m_hoa.value_or(k_declined) ==
                   unbounded.m_hoa.value_or(k_declined),
               "spot-translate: a deadline must not change the automaton for " +
                   formula);
    }
}

// Determinizing this takes about 4.7 seconds and 50MB, measured, and both
// figures are why it is the one used here. It has to be slow enough to miss a
// 250ms deadline on any machine -- a nineteenfold margin -- and it has to
// finish soon enough afterwards for the recovery below to be checked rather
// than merely waited out. Related shapes are far worse: the same formula over
// four GF-implication pairs runs past three minutes, which would make this a
// test that hangs the suite instead of one that measures it.
const char* const k_slow_formula =
    "!((G F a0) <-> (G F a1) <-> (G F a2) <-> (G F a3) <-> (G F a4) <-> "
    "(G F a5))";

// The whole point of the deadline, and of the three things it has to get right,
// this is the second: the call returns. It cannot be cancelled, so returning
// means abandoning, and what has to be checked is that abandoning is bounded --
// that the process is left slower rather than stuck.
//
// Deliberately left as one test rather than three. Each stage is only
// observable through the state the previous one left behind, and splitting them
// would leave an abandoned worker running across test boundaries, which is
// exactly the condition the last stage exists to clear.
void test_a_missed_deadline_is_abandoned_and_then_recovered_from() {
    const std::size_t before = spot_abandoned_workers();
    const auto start = std::chrono::steady_clock::now();
    const SpotTranslation translation = spot_translate_for_counting(
        k_slow_formula, k_budget, std::chrono::milliseconds(250));
    const auto waited = std::chrono::steady_clock::now() - start;

    expect(translation.m_timed_out,
           "spot-translate: a translation past its deadline must report a "
           "timeout rather than an automaton");
    expect(!translation.m_hoa.has_value(),
           "spot-translate: a timed-out translation carries no automaton");
    // The deadline is only worth anything if it is the caller's wall time and
    // not the translation's. Ten seconds is far below the twenty-odd the
    // formula needs and far above the 250ms asked for, so this fails only if
    // the call actually waited for the work.
    expect(waited < std::chrono::seconds(10),
           "spot-translate: the caller must return on the deadline, not when "
           "the abandoned translation finishes");
    expect(spot_abandoned_workers() == before + 1,
           "spot-translate: the abandoned worker should be counted, so the "
           "process can tell it is running before it exits");

    // The abandoned worker still holds libspot, so the fast path is gone for as
    // long as it runs. It has to degrade to spawning the tool -- if the lock
    // were instead waited on, one bad formula would stall every scoring thread
    // in the process rather than just slowing it down.
    const SpotSimplification during =
        spot_try_simplify("G(p -> F(q))", k_budget);
    expect(during.m_lock_busy,
           "spot-simplify: while a worker is abandoned the lock must report "
           "busy within the budget, sending callers to the tool");

    // And it has to come back on its own. A worker that finished without
    // releasing libspot would leave the process permanently on the slow path,
    // which is a performance bug nothing else here would catch.
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(120);
    while (spot_abandoned_workers() != before &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    expect(spot_abandoned_workers() == before,
           "spot-translate: an abandoned worker must eventually finish and "
           "stop being counted");
    const SpotSimplification after =
        spot_try_simplify("G(p -> F(q))", k_budget);
    expect(!after.m_lock_busy,
           "spot-simplify: once the abandoned worker finishes, the in-process "
           "path must be available again");
}

}  // namespace

void run_spot_inprocess_tests() {
    test_agrees_with_ltlfilt();
    test_simplification_level_matches_the_flag();
    test_unparseable_formula_is_declined();
    test_concurrent_calls_agree();
    test_budgeted_simplify_agrees_when_uncontended();
    test_budgeted_simplify_declines_unparseable();
    test_translation_matches_ltl2tgba();
    test_unparseable_formula_is_declined_by_translate();
    test_tautology_print_bug_is_reported();
    test_a_deadline_does_not_change_the_answer();
    test_a_missed_deadline_is_abandoned_and_then_recovered_from();
}
