#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

#include "config.hpp"
#include "genetic/random_source.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"
#include "tlsf/filter.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/pipeline.hpp"
#include "tlsf/specification.hpp"

namespace {

// A two-client mutual-exclusion GR(1) arbiter that is unrealizable without a
// request-fairness assumption: the environment can hold both requests low
// forever, so the system cannot satisfy `G F g0` / `G F g1` while honouring
// `G(g -> r)`.
const char* const k_unrealizable =
    "INFO { SEMANTICS: Mealy; }\n"
    "MAIN {\n"
    "  INPUTS { r0; r1; }\n"
    "  OUTPUTS { g0; g1; }\n"
    "  GUARANTEE {\n"
    "    G (g0 -> r0);\n"
    "    G (g1 -> r1);\n"
    "    G !(g0 & g1);\n"
    "    G F g0;\n"
    "    G F g1;\n"
    "  }\n"
    "}\n";

// The same arbiter with the missing fairness assumptions restored.
const char* const k_realizable =
    "INFO { SEMANTICS: Mealy; }\n"
    "MAIN {\n"
    "  INPUTS { r0; r1; }\n"
    "  OUTPUTS { g0; g1; }\n"
    "  ASSUME { G F r0; G F r1; }\n"
    "  GUARANTEE {\n"
    "    G (g0 -> r0);\n"
    "    G (g1 -> r1);\n"
    "    G !(g0 & g1);\n"
    "    G F g0;\n"
    "    G F g1;\n"
    "  }\n"
    "}\n";

bool is_realizable(const tlsf::Specification& spec) {
    // No timeout is set in the tests, so every query is decided; value_or's
    // argument is unreachable rather than a policy choice.
    return global_real_checker()
        .check_realizability_ltl(spec.to_ltl(), spec.m_inputs, spec.m_outputs)
        .value_or(false);
}

void test_arbiter_realizability() {
    const tlsf::Specification unrealizable = tlsf::parse(k_unrealizable);
    const tlsf::Specification realizable = tlsf::parse(k_realizable);
    expect(!is_realizable(unrealizable),
           "arbiter: spec without fairness is unrealizable");
    expect(is_realizable(realizable),
           "arbiter: spec with fairness assumptions is realizable");
}

void test_run_repair_end_to_end() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("tlsf_pipeline_test_" +
         std::to_string(std::hash<std::string>{}(std::string(k_unrealizable))));
    std::error_code err_code;
    std::filesystem::remove_all(dir, err_code);
    expect(std::filesystem::create_directories(dir, err_code),
           "pipeline: temp directory is created");

    const std::filesystem::path input_path = dir / "spec.tlsf";
    {
        std::ofstream input(input_path);
        input << k_unrealizable;
    }

    Config cfg;
    cfg.generations = 2;
    cfg.population_size = 20;
    cfg.parallel = 1;
    cfg.default_model_counting_bound = 3;

    const RandomSource random_source = make_random_source_from_seed(1234);
    SearchBudget budget(cfg, SearchBudget::Clock::now());
    const int status = tlsf::run_repair(input_path.string(), dir.string(), cfg,
                                        random_source, budget);
    expect(status == 0, "pipeline: run_repair returns 0");

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".tlsf" ||
            entry.path().filename() == "spec.tlsf") {
            continue;
        }
        std::ifstream repair(entry.path());
        std::ostringstream contents;
        contents << repair.rdbuf();
        const tlsf::Specification spec = tlsf::parse(contents.str());
        expect(is_realizable(spec),
               "pipeline: each written repair re-parses and is realizable");

        // Each repair carries a sidecar fitness record with the weighted total
        // and the per-objective breakdown, mirroring the FRETISH output.
        const std::filesystem::path fitness_path =
            entry.path().parent_path() /
            (entry.path().stem().string() + ".fitness.json");
        expect(std::filesystem::exists(fitness_path),
               "pipeline: each repair has a .fitness.json sidecar");
        std::ifstream fitness_stream(fitness_path);
        std::ostringstream fitness_contents;
        fitness_contents << fitness_stream.rdbuf();
        const nlohmann::json record =
            nlohmann::json::parse(fitness_contents.str());
        expect(record.contains("total") && record.at("total").is_number(),
               "pipeline: fitness record has a numeric total");
        expect(record.contains("components") &&
                   record.at("components").is_array() &&
                   !record.at("components").empty(),
               "pipeline: fitness record lists per-objective components");
        for (const nlohmann::json& component : record.at("components")) {
            expect(component.contains("name") &&
                       component.at("name").is_string() &&
                       component.contains("score") &&
                       component.at("score").is_number() &&
                       component.contains("weight") &&
                       component.at("weight").is_number(),
                   "pipeline: each component has name, score, and weight");
        }
    }

    std::filesystem::remove_all(dir, err_code);
}

// MUC-mode repair on the same unrealizable arbiter: the iterative
// extract-repair-reintegrate loop must converge to a written, realizable
// repair. gen5/pop50/bound3 at seed 0 is the smallest budget observed to
// repair this fixture reliably across seeds.
void test_muc_repair_end_to_end() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("tlsf_muc_test_" +
         std::to_string(std::hash<std::string>{}(std::string(k_unrealizable))));
    std::error_code err_code;
    std::filesystem::remove_all(dir, err_code);
    expect(std::filesystem::create_directories(dir, err_code),
           "muc: temp directory is created");

    const std::filesystem::path input_path = dir / "spec.tlsf";
    {
        std::ofstream input(input_path);
        input << k_unrealizable;
    }

    Config cfg;
    cfg.generations = 5;
    cfg.population_size = 50;
    cfg.parallel = 1;
    cfg.default_model_counting_bound = 3;
    cfg.repair_mode = RepairMode::Muc;
    // This test is about the MUC loop converging to a realizable output, and
    // whether that output is also a logical weakening is a separate question --
    // see test_weakening_screen_rejects_non_weakening below, which pins the
    // screen on a seed that reaches a non-weakening repair. Leaving the screen
    // on here would assert two things at once and make this test hostage to
    // which repair the seed happens to reach.
    cfg.run_weakening_filter = false;

    const RandomSource random_source = make_random_source_from_seed(0);
    SearchBudget budget(cfg, SearchBudget::Clock::now());
    const int status = tlsf::run_repair(input_path.string(), dir.string(), cfg,
                                        random_source, budget);
    expect(status == 0, "muc: run_repair returns 0");

    std::size_t n_repairs = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".tlsf" ||
            entry.path().filename() == "spec.tlsf") {
            continue;
        }
        std::ifstream repair(entry.path());
        std::ostringstream contents;
        contents << repair.rdbuf();
        const tlsf::Specification spec = tlsf::parse(contents.str());
        expect(is_realizable(spec),
               "muc: each written repair re-parses and is realizable");
        ++n_repairs;
    }
    expect(n_repairs >= 1, "muc: repair produced at least one realizable spec");

    std::filesystem::remove_all(dir, err_code);
}

// Reads back every repair a run wrote, ignoring the input specification when
// the run was pointed at the directory holding it.
std::vector<tlsf::Specification> read_repairs(
    const std::filesystem::path& dir) {
    std::vector<tlsf::Specification> repairs;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".tlsf" ||
            entry.path().filename() == "spec.tlsf") {
            continue;
        }
        std::ifstream repair(entry.path());
        std::ostringstream contents;
        contents << repair.rdbuf();
        repairs.push_back(tlsf::parse(contents.str()));
    }
    return repairs;
}

// The screen runs after the search, over whatever realizable survivors it
// reached (`keep_weakenings` in src/tlsf/pipeline.cpp), so the same fixture at
// the same seed run with the screen off and then on differs only in which of
// those survivors reach disk. The pair is a direct oracle for the screen's
// decision on every candidate the search actually produced: kept must mean
// implied by the original, dropped must mean not implied. Neither half depends
// on *which* candidates those are.
//
// That independence is the point. This test asserted `n_repairs == 0` until
// issue #139 -- that the seed reaches a repair the screen will reject, which is
// a fact about the search rather than about the screen. Ten of the first 24
// seeds reach a genuine weakening on x86-64, and seed 1 reaches one on arm64,
// where `Count` is binary64 rather than 80-bit x87. The screen was correct in
// every one of those cases and the assertion failed anyway.
//
// Unlike the FRETISH assume-guarantee decomposition, tlsf_spec_implies is an
// exact whole-formula query, so a verdict here is a fact about the two specs
// rather than an artefact of how the check decomposes them.
void test_weakening_screen_keeps_exactly_the_weakenings() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("tlsf_weakening_test_" +
         std::to_string(std::hash<std::string>{}(std::string(k_unrealizable))));
    std::error_code err_code;
    std::filesystem::remove_all(dir, err_code);
    expect(std::filesystem::create_directories(dir, err_code),
           "weakening: temp directory is created");

    const std::filesystem::path input_path = dir / "spec.tlsf";
    {
        std::ofstream input(input_path);
        input << k_unrealizable;
    }

    const auto run_with_screen =
        [&](bool screen) -> std::vector<tlsf::Specification> {
        const std::filesystem::path out = dir / (screen ? "on" : "off");
        expect(std::filesystem::create_directories(out, err_code),
               "weakening: run output directory is created");

        Config cfg;
        cfg.generations = 5;
        cfg.population_size = 50;
        cfg.parallel = 1;
        cfg.default_model_counting_bound = 3;
        cfg.repair_mode = RepairMode::Muc;
        cfg.run_weakening_filter = screen;
        // Pinned rather than left at the default so both runs agree on the
        // survivor step, which is what makes them comparable at all.
        cfg.selection_scheme = SelectionScheme::Nsga2Truncate;

        const RandomSource random_source = make_random_source_from_seed(1);
        SearchBudget budget(cfg, SearchBudget::Clock::now());
        const int status = tlsf::run_repair(input_path.string(), out.string(),
                                            cfg, random_source, budget);
        expect(status == 0, "weakening: run_repair returns 0");
        return read_repairs(out);
    };

    const std::vector<tlsf::Specification> unscreened = run_with_screen(false);
    const std::vector<tlsf::Specification> screened = run_with_screen(true);

    // Without this the loop below is vacuously true. It asserts only that the
    // search reaches something, which is far weaker than asserting what.
    expect(!unscreened.empty(),
           "weakening: the fixture reaches at least one realizable repair");
    expect(screened.size() <= unscreened.size(),
           "weakening: the screen only removes repairs");

    const tlsf::Specification original = tlsf::parse(k_unrealizable);
    SatisfiabilityChecker& checker = global_sat_checker();
    const auto was_kept = [&screened](const tlsf::Specification& spec) {
        return std::any_of(
            screened.begin(), screened.end(),
            [&spec](const tlsf::Specification& kept) { return kept == spec; });
    };
    for (const tlsf::Specification& spec : unscreened) {
        // The screen keeps an undecided candidate, so value_or(true) reproduces
        // its own policy rather than being a convenience: kept means implied or
        // undecided, dropped means decided and not implied.
        const bool weakening =
            tlsf_spec_implies(original, spec, checker).value_or(true);
        if (was_kept(spec)) {
            expect(weakening,
                   "weakening: every repair the screen kept is implied by the "
                   "original");
        } else {
            expect(!weakening,
                   "weakening: every repair the screen dropped is not implied "
                   "by the original");
        }
    }

    std::filesystem::remove_all(dir, err_code);
}

}  // namespace

void run_tlsf_pipeline_tests() {
    test_arbiter_realizability();
    test_muc_repair_end_to_end();
    test_weakening_screen_keeps_exactly_the_weakenings();
    test_run_repair_end_to_end();
}
