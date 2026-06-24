#include <cstddef>
#include <stdexcept>
#include <string>

#include "config.hpp"
#include "config_io.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

void test_config_io_all_fields() {
    const std::string toml = R"(
[genetic]
generations     = 5
population_size = 100
crossover_rate  = 0.2
mutation_rate   = 0.8

[fitness]
weight_syntactic = 0.3
weight_semantic  = 0.4
weight_halstead  = 0.05
weight_status    = 0.6

[syntactic]
weight_trigger  = 2.0
weight_response = 1.5
weight_timing   = 0.5

[mutation]
p_trigger  = 0.3
p_response = 0.7
p_timing   = 0.1

[model_counting]
default_bound = 10

[filters]
run_weakening   = false
run_implication = false

[runtime]
black_timeout_ms = 500
parallel         = 4
)";
    const Config cfg = config_from_toml_string(toml);
    expect(cfg.generations == 5,
           "config_io: generations should be parsed from TOML");
    expect(cfg.population_size == 100,
           "config_io: population_size should be parsed from TOML");
    expect(cfg.crossover_rate == 0.2,
           "config_io: crossover_rate should be parsed from TOML");
    expect(cfg.mutation_rate == 0.8,
           "config_io: mutation_rate should be parsed from TOML");
    expect(cfg.fitness_weight_syntactic == 0.3,
           "config_io: fitness weight_syntactic should be parsed from TOML");
    expect(cfg.fitness_weight_semantic == 0.4,
           "config_io: fitness weight_semantic should be parsed from TOML");
    expect(cfg.fitness_weight_halstead == 0.05,
           "config_io: fitness weight_halstead should be parsed from TOML");
    expect(cfg.fitness_weight_status == 0.6,
           "config_io: fitness weight_status should be parsed from TOML");
    expect(cfg.syntactic_weight_trigger == 2.0,
           "config_io: syntactic weight_trigger should be parsed from TOML");
    expect(cfg.syntactic_weight_response == 1.5,
           "config_io: syntactic weight_response should be parsed from TOML");
    expect(cfg.syntactic_weight_timing == 0.5,
           "config_io: syntactic weight_timing should be parsed from TOML");
    expect(cfg.p_trigger == 0.3,
           "config_io: mutation p_trigger should be parsed from TOML");
    expect(cfg.p_response == 0.7,
           "config_io: mutation p_response should be parsed from TOML");
    expect(cfg.p_timing == 0.1,
           "config_io: mutation p_timing should be parsed from TOML");
    expect(cfg.default_model_counting_bound == 10,
           "config_io: model_counting.default_bound should be parsed");
    expect(!cfg.run_weakening_filter,
           "config_io: filters run_weakening should be false");
    expect(!cfg.run_implication_filter,
           "config_io: filters run_implication should be false");
    expect(cfg.black_timeout == std::chrono::milliseconds{500},
           "config_io: runtime black_timeout_ms should be parsed from TOML");
    expect(cfg.parallel == 4,
           "config_io: runtime parallel should be parsed from TOML");
}

void test_config_io_partial_overrides_defaults() {
    const std::string toml = R"(
[genetic]
generations = 5
)";
    const Config cfg = config_from_toml_string(toml);
    const Config defaults;
    expect(cfg.generations == 5,
           "config_io: partial TOML should override only specified fields");
    expect(cfg.population_size == defaults.population_size,
           "config_io: unspecified population_size should remain default");
    expect(cfg.crossover_rate == defaults.crossover_rate,
           "config_io: unspecified crossover_rate should remain default");
}

void test_config_io_missing_file_throws() {
    bool threw = false;
    try {
        config_from_toml("/tmp/counter_test_nonexistent_config.toml");
    } catch (const std::exception& exc) {
        threw = true;
        const std::string msg(exc.what());
        expect(msg.find("does not exist") != std::string::npos,
               "config_io: missing file error should mention 'does not exist'");
    }
    expect(threw, "config_io: missing file should throw");
}

void test_config_io_invalid_toml_throws() {
    bool threw = false;
    try {
        config_from_toml_string("this is not valid toml ===");
    } catch (const std::exception&) {
        threw = true;
    }
    expect(threw, "config_io: invalid TOML should throw");
}

void test_config_io_out_of_range_probability_throws() {
    bool threw = false;
    try {
        config_from_toml_string("[mutation]\np_trigger = 1.5\n");
    } catch (const std::exception& exc) {
        threw = true;
        const std::string msg(exc.what());
        expect(msg.find("p_trigger") != std::string::npos,
               "config_io: out-of-range error should name the field");
    }
    expect(threw, "config_io: out-of-range probability should throw");
}

void test_config_io_empty_string_gives_defaults() {
    const Config cfg = config_from_toml_string("");
    const Config defaults;
    expect(cfg.generations == defaults.generations,
           "config_io: empty TOML should give default generations");
    expect(cfg.population_size == defaults.population_size,
           "config_io: empty TOML should give default population_size");
}

}  // namespace

void run_config_io_tests() {
    test_config_io_all_fields();
    test_config_io_partial_overrides_defaults();
    test_config_io_missing_file_throws();
    test_config_io_invalid_toml_throws();
    test_config_io_out_of_range_probability_throws();
    test_config_io_empty_string_gives_defaults();
}
