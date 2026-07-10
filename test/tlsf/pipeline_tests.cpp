#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include "config.hpp"
#include "genetic/random_source.hpp"
#include "runner/spot.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"
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
    return global_real_checker().check_realizability_ltl(
        spec.to_ltl(), spec.m_inputs, spec.m_outputs);
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
    const int status =
        tlsf::run_repair(input_path.string(), dir.string(), cfg, random_source);
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
    }

    std::filesystem::remove_all(dir, err_code);
}

}  // namespace

void run_tlsf_pipeline_tests() {
    test_arbiter_realizability();
    test_run_repair_end_to_end();
}
