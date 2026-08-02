// Differential tests: the in-process libspot paths against the command-line
// tools they replaced, over a corpus rather than a handful of examples.
//
// The rest of the suite pins behaviour on formulae chosen because someone
// thought of them. That catches a path that is broken; it does not catch one
// that is subtly different, which is the failure mode these replacements
// actually have. A simplifier that disagrees with `ltlfilt --simplify` on one
// formula in a thousand does not fail anything -- it changes fitness scores,
// and the run still finishes and still writes repairs. The only way that
// surfaces is as results that do not match an earlier campaign's, months later,
// with nothing to point at.
//
// So the corpus is generated rather than written: randltl at a fixed seed,
// across several shapes and alphabet sizes, plus the patterns this project
// builds itself (deep nested X, W, timing windows) which random generation
// rarely produces.
//
// Size is set by COUNTER_DIFFERENTIAL_N, defaulting to something a normal ctest
// run can afford. Every comparison costs an exec of the tool being compared
// against, so the default is not where the confidence comes from -- it is a
// regression guard, and the number is meant to be raised deliberately:
//
//     COUNTER_DIFFERENTIAL_N=5000 ./counter_tests differential

#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "runner/ltlfilt.hpp"
#include "runner/spot.hpp"
#include "runner/spot_inprocess.hpp"
#include "runner/subprocess.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

constexpr std::chrono::milliseconds k_budget{8};
constexpr std::chrono::milliseconds k_no_deadline{0};

// Translation is much dearer than simplification -- an exec plus a
// determinization each -- so it runs over a fraction of the corpus rather than
// all of it. The fraction is what keeps the default run affordable; raising
// COUNTER_DIFFERENTIAL_N raises both.
constexpr std::size_t k_translate_divisor = 4;

std::size_t corpus_size() {
    const char* requested = std::getenv("COUNTER_DIFFERENTIAL_N");
    if (requested == nullptr) {
        return 400;
    }
    const std::int64_t parsed = std::strtoll(requested, nullptr, 10);
    return parsed > 0 ? static_cast<std::size_t>(parsed) : 400;
}

std::string randltl_path() { return spot_bin_dir() + "/randltl"; }

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        if (end > start) {
            lines.push_back(text.substr(start, end - start));
        }
        start = end + 1;
    }
    return lines;
}

// The shapes this project generates and randltl does not. Deep nested X comes
// from WithinTicks over a long horizon, W from the Continual condition type,
// and the trivially-true and trivially-false cases are what a mutation lands on
// often enough to matter. Kept in the corpus verbatim so a disagreement on the
// formulae actually being simplified cannot hide behind random ones.
std::vector<std::string> project_shapes() {
    std::vector<std::string> shapes = {
        "G(a -> X(X(X(X(X(b))))))",
        "G(a -> (b W c))",
        "G((a & b) -> X(c U (d & Xe)))",
        "a & !a",
        "a | !a",
        "G(true)",
        "G(false)",
        "G((a & !((b & !c) -> d)) -> b)",
        "b | G(Fe U Gc)",
        "Xc & G(!b & (c W b))",
        "a <-> F(a | GX!Fa)",
        "G(F(a)) & G(F(b)) & G(F(c))",
        "(G(a) | F(b)) U (c & X(d))",
    };
    for (int depth = 6; depth <= 18; depth += 4) {
        std::string nested = "z";
        for (int i = 0; i < depth; ++i) {
            nested.insert(0, "X(").append(")");
        }
        shapes.push_back("G(y -> " + nested + ")");
    }
    return shapes;
}

// Generated once and shared by every test below, because randltl costs an exec
// per call and the corpus has to be the same across tests for a disagreement in
// one to be comparable with the others.
const std::vector<std::string>& corpus() {
    static const std::vector<std::string> built = [] {
        std::vector<std::string> formulas = project_shapes();
        const std::size_t target = corpus_size();
        // Several alphabets and tree sizes rather than one: a single randltl
        // call produces formulae of one shape, and the disagreements worth
        // finding are as likely to be about how many atoms interact as about
        // depth.
        const std::vector<std::pair<const char*, const char*>> shapes = {
            {"2", "10..20"}, {"3", "15..30"}, {"4", "20..45"}, {"6", "25..60"}};
        std::size_t seed = 1;
        for (const auto& [atoms, tree_size] : shapes) {
            const std::size_t want = (target / shapes.size()) + 1;
            std::vector<std::string> args = {
                randltl_path(),
                "-n",
                std::to_string(want),
                std::string("--tree-size=") + tree_size,
                std::string("--seed=") + std::to_string(seed++),
                atoms};
            const SubprocessResult result = run_subprocess(args);
            for (std::string& line : split_lines(result.m_output)) {
                formulas.push_back(std::move(line));
            }
        }
        if (formulas.size() > target + project_shapes().size()) {
            formulas.resize(target + project_shapes().size());
        }
        return formulas;
    }();
    return built;
}

std::string via_ltlfilt_simplify(const std::string& formula) {
    const SubprocessResult result =
        run_subprocess({ltlfilt_path(), "--simplify", "-f", formula});
    std::string output = result.m_output;
    while (!output.empty() && output.back() == '\n') {
        output.pop_back();
    }
    return output;
}

// ltl2tgba writes its own simplified rendering of the formula into `name:`.
// Nothing here reads it, and it is the one line the in-process path does not
// reproduce, so it is dropped before comparing.
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

// Language equivalence of two automata, via autfilt. Both have to reach it as
// files -- autfilt reads automata from paths, not arguments -- so they are
// written to a directory this process owns and overwritten per call rather than
// accumulating one pair per formula.
bool automata_equivalent(const std::string& lhs, const std::string& rhs) {
    static const std::string dir = [] {
        const std::string path =
            std::filesystem::temp_directory_path() /
            ("counter_differential_" + std::to_string(::getpid()));
        std::filesystem::create_directories(path);
        return path;
    }();
    const std::string lhs_path = dir + "/lhs.hoa";
    const std::string rhs_path = dir + "/rhs.hoa";
    {
        std::ofstream lhs_file(lhs_path);
        lhs_file << lhs;
        std::ofstream rhs_file(rhs_path);
        rhs_file << rhs;
    }
    const SubprocessResult result = run_subprocess(
        {spot_bin_dir() + "/autfilt", "--equivalent-to=" + lhs_path, rhs_path});
    // 0 means the input matched the filter, so the two accept the same
    // language; 1 means it did not. Anything else is autfilt failing to answer,
    // which must not be read as agreement.
    return result.m_exit_code == 0;
}

// One message per test rather than per formula. A corpus run that disagrees on
// hundreds of formulae should say so once, with an example, instead of burying
// the count under hundreds of identical-looking lines.
void report(const std::string& what, std::size_t checked,
            std::size_t disagreements, const std::string& first_example) {
    expect(disagreements == 0,
           what + ": " + std::to_string(disagreements) + " of " +
               std::to_string(checked) +
               " formulae disagreed. First: " + first_example);
}

// What in-process simplification can promise the tool, and what it cannot.
//
// It cannot promise the same bytes. SPOT orders the operands of commutative
// operators by formula-node id, and ids are handed out in the order the process
// first interned each node -- so the printed form depends on what the process
// simplified *earlier*. `ltlfilt` looked like a pure function only because
// every call got a fresh process and therefore a fresh table. Measured here:
// about a fifth of a random corpus prints differently, and the same formula
// simplified in a cold process agrees with the tool while the identical call
// after a handful of unrelated ones does not.
//
// What it can promise is that the answer means the same thing, which is what
// every caller of simplify_ltl actually depends on. That is asserted here; the
// consequences of the ordering are pinned by the test below and measured
// end-to-end in scripts/check_engine_parity.py.
void test_simplify_is_equivalent_to_the_tool_over_the_corpus() {
    std::size_t disagreements = 0;
    std::string first;
    for (const std::string& formula : corpus()) {
        const std::string in_process = spot_simplify(formula).value_or("");
        const std::string from_tool = via_ltlfilt_simplify(formula);
        if (in_process == from_tool) {
            continue;
        }
        // A formula libspot declined but the tool simplified is a real
        // disagreement, and would fail the equivalence check below anyway --
        // but as an empty string rather than as anything readable, so it is
        // separated out to say what happened.
        if (in_process.empty() || from_tool.empty()) {
            ++disagreements;
            if (first.empty()) {
                first.append("\"").append(formula).append(
                    "\" -> one side declined it: libspot \"");
                first.append(in_process).append("\" vs ltlfilt \"");
                first.append(from_tool).append("\"");
            }
            continue;
        }
        if (!ltl_equivalent(in_process, from_tool)) {
            ++disagreements;
            if (first.empty()) {
                first.append("\"").append(formula).append("\" -> libspot \"");
                first.append(in_process)
                    .append(
                        "\" is not equivalent to "
                        "ltlfilt \"");
                first.append(from_tool).append("\"");
            }
        }
    }
    report("differential/simplify-equivalence", corpus().size(), disagreements,
           first);
}

// The ordering above is a reordering and nothing more. If it were hiding a
// genuine difference -- a rewrite one side makes and the other does not -- then
// feeding the tool's answer back through the in-process simplifier would not
// land on the in-process answer for the original.
//
// Equivalence alone would not catch that: two formulae can mean the same thing
// and still be simplified differently, and a simplifier that quietly stopped
// applying a rule would keep passing the test above while making every fitness
// score in a campaign slightly worse.
void test_the_difference_from_the_tool_is_only_ordering() {
    std::size_t checked = 0;
    std::size_t disagreements = 0;
    std::string first;
    for (const std::string& formula : corpus()) {
        const std::string in_process = spot_simplify(formula).value_or("");
        const std::string from_tool = via_ltlfilt_simplify(formula);
        if (in_process == from_tool || in_process.empty() ||
            from_tool.empty()) {
            continue;
        }
        ++checked;
        // Same process, so the same intern table decides the ordering both
        // times: the two must print identically or the difference was more
        // than ordering.
        const std::string round_tripped = spot_simplify(from_tool).value_or("");
        if (round_tripped != in_process) {
            ++disagreements;
            if (first.empty()) {
                first.append("\"").append(formula).append(
                    "\" -> re-simplifying ltlfilt's \"");
                first.append(from_tool).append("\" gave \"");
                first.append(round_tripped).append("\", not libspot's \"");
                first.append(in_process).append("\"");
            }
        }
    }
    report("differential/simplify-ordering-only", checked, disagreements,
           first);
}

void test_translate_agrees_with_the_tool_over_the_corpus() {
    std::size_t checked = 0;
    std::size_t disagreements = 0;
    std::string first;
    for (std::size_t i = 0; i < corpus().size(); i += k_translate_divisor) {
        const std::string& formula = corpus()[i];
        const SpotTranslation translation =
            spot_translate_for_counting(formula, k_budget, k_no_deadline);
        const SubprocessResult from_tool =
            run_subprocess({ltl2tgba_path(), "-D", "-S", "-H", "-f", formula});
        ++checked;
        // The tool's exit 2 on a tautology is the SPOT print bug, which the
        // in-process path reports as m_tautology_print_bug rather than as an
        // automaton. Agreeing there means agreeing that the formula is a
        // tautology, not agreeing on bytes.
        if (from_tool.m_exit_code == 2) {
            if (!translation.m_tautology_print_bug) {
                ++disagreements;
                if (first.empty()) {
                    first = "\"" + formula +
                            "\" -> ltl2tgba reports the tautology print bug "
                            "and libspot does not";
                }
            }
            continue;
        }
        const std::string in_process =
            strip_name_line(translation.m_hoa.value_or(""));
        const std::string tool_hoa = strip_name_line(from_tool.m_output);
        if (in_process == tool_hoa) {
            continue;
        }
        // Same intern table, same reason as for simplification: the atomic
        // propositions come out in the order the process first saw them, so a
        // warm process lists `AP:` differently and renumbers every edge label
        // to match. That is a renaming, not a different automaton. Comparing
        // bytes would fail on it while still missing an automaton that had
        // genuinely lost a state; autfilt decides the question the counting
        // path actually asks, which is whether the two accept the same
        // language.
        if (!automata_equivalent(in_process, tool_hoa)) {
            ++disagreements;
            if (first.empty()) {
                first.append("\"").append(formula).append(
                    "\" -> the in-process automaton is not equivalent to "
                    "ltl2tgba's");
            }
        }
    }
    report("differential/translate", checked, disagreements, first);
}

// The deadline path runs the translation on another thread. Nothing about that
// should be visible in the answer, and this is the only test that would notice
// if it were -- a campaign configured with a timeout would simply produce
// different numbers from one without.
void test_a_deadline_does_not_change_the_corpus() {
    std::size_t checked = 0;
    std::size_t disagreements = 0;
    std::string first;
    for (std::size_t i = 0; i < corpus().size(); i += k_translate_divisor) {
        const std::string& formula = corpus()[i];
        const SpotTranslation inlined =
            spot_translate_for_counting(formula, k_budget, k_no_deadline);
        const SpotTranslation deadlined = spot_translate_for_counting(
            formula, k_budget, std::chrono::seconds(120));
        ++checked;
        if (deadlined.m_timed_out) {
            ++disagreements;
            if (first.empty()) {
                first = "\"" + formula +
                        "\" -> timed out against a 120s "
                        "deadline it should not have missed";
            }
            continue;
        }
        if (inlined.m_hoa != deadlined.m_hoa ||
            inlined.m_tautology_print_bug != deadlined.m_tautology_print_bug) {
            ++disagreements;
            if (first.empty()) {
                first = "\"" + formula +
                        "\" -> inline and deadlined "
                        "translations differ";
            }
        }
    }
    report("differential/deadline", checked, disagreements, first);
}

// Concurrency is where a shared-state bug in libspot would show, and it would
// show as a wrong answer rather than a crash: the intern table is global, and a
// torn read of it produces a valid formula, just not the right one. Every
// thread simplifies the whole corpus, and every thread has to reach the same
// answers the single-threaded pass did.
void test_concurrent_simplification_agrees_with_serial() {
    std::vector<std::string> serial;
    serial.reserve(corpus().size());
    for (const std::string& formula : corpus()) {
        serial.push_back(spot_simplify(formula).value_or(""));
    }

    const std::size_t n_threads = 8;
    std::vector<std::vector<std::string>> per_thread(n_threads);
    std::vector<std::thread> threads;
    threads.reserve(n_threads);
    for (std::size_t ti = 0; ti < n_threads; ++ti) {
        threads.emplace_back([&per_thread, ti] {
            per_thread[ti].reserve(corpus().size());
            for (const std::string& formula : corpus()) {
                // Through the budgeted entry point, which is what the scoring
                // threads use: under this much contention most calls will find
                // the lock busy, so this also covers the path that gives up.
                SpotSimplification result =
                    spot_try_simplify(formula, k_budget);
                if (result.m_lock_busy) {
                    result.m_formula = spot_simplify(formula);
                }
                per_thread[ti].push_back(result.m_formula.value_or(""));
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }

    std::size_t disagreements = 0;
    std::string first;
    for (std::size_t ti = 0; ti < n_threads; ++ti) {
        for (std::size_t index = 0; index < serial.size(); ++index) {
            if (per_thread[ti][index] != serial[index]) {
                ++disagreements;
                if (first.empty()) {
                    first.append("\"").append(corpus()[index]);
                    first.append("\" -> thread ").append(std::to_string(ti));
                    first.append(" got \"").append(per_thread[ti][index]);
                    first.append("\", serial got \"").append(serial[index]);
                    first.append("\"");
                }
            }
        }
    }
    report("differential/concurrent-simplify", serial.size() * n_threads,
           disagreements, first);
}

}  // namespace

void run_differential_tests() {
    test_simplify_is_equivalent_to_the_tool_over_the_corpus();
    test_the_difference_from_the_tool_is_only_ordering();
    test_translate_agrees_with_the_tool_over_the_corpus();
    test_a_deadline_does_not_change_the_corpus();
    test_concurrent_simplification_agrees_with_serial();
}
