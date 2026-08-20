#include <cstddef>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "config.hpp"
#include "config_io.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

Config config_capturing_warnings(const std::string& toml,
                                 std::string& warnings) {
    std::ostringstream captured;
    std::streambuf* const previous = std::cerr.rdbuf(captured.rdbuf());
    try {
        Config cfg = config_from_toml_string(toml);
        std::cerr.rdbuf(previous);
        warnings = captured.str();
        return cfg;
    } catch (...) {
        std::cerr.rdbuf(previous);
        warnings = captured.str();
        throw;
    }
}

std::string warnings_from(const std::string& toml) {
    std::string warnings;
    config_capturing_warnings(toml, warnings);
    return warnings;
}

void test_config_io_all_fields() {
    const std::string toml = R"(
[genetic]
generations     = 5
population_size = 100
crossover_rate  = 0.2
mutation_rate   = 0.8
accumulate_repairs = true
repaired_operators = true

[fitness]
weight_syntactic = 0.3
weight_semantic  = 0.4
weight_status    = 0.6

[mutation]
p_trigger  = 0.3
p_response = 0.7
p_timing   = 0.1

[model_counting]
default_bound = 10

[filters]
run_weakening       = true
run_implication     = false
run_well_separation = true

[runtime]
black_timeout_ms = 500
parallel         = 4
dashboard        = true
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
    expect(cfg.accumulate_repairs,
           "config_io: accumulate_repairs should be parsed from TOML");
    expect(!Config{}.accumulate_repairs,
           "config_io: accumulate_repairs must default to false, or every "
           "archived config silently means something new");
    expect(cfg.repaired_operators,
           "config_io: repaired_operators should be parsed from TOML");
    expect(!Config{}.repaired_operators,
           "config_io: repaired_operators must default to false, or every "
           "archived config silently means something new");
    expect(cfg.fitness_weight_syntactic == 0.3,
           "config_io: fitness weight_syntactic should be parsed from TOML");
    expect(cfg.fitness_weight_semantic == 0.4,
           "config_io: fitness weight_semantic should be parsed from TOML");
    expect(cfg.fitness_weight_status == 0.6,
           "config_io: fitness weight_status should be parsed from TOML");
    expect(cfg.p_trigger == 0.3,
           "config_io: mutation p_trigger should be parsed from TOML");
    expect(cfg.p_response == 0.7,
           "config_io: mutation p_response should be parsed from TOML");
    expect(cfg.p_timing == 0.1,
           "config_io: mutation p_timing should be parsed from TOML");
    expect(cfg.default_model_counting_bound == 10,
           "config_io: model_counting.default_bound should be parsed");
    expect(cfg.run_weakening_filter,
           "config_io: filters run_weakening should be true");
    expect(!cfg.run_implication_filter,
           "config_io: filters run_implication should be false");
    expect(cfg.run_well_separation_filter,
           "config_io: filters run_well_separation should be true");
    expect(cfg.black_timeout == std::chrono::milliseconds{500},
           "config_io: runtime black_timeout_ms should be parsed from TOML");
    expect(cfg.parallel == 4,
           "config_io: runtime parallel should be parsed from TOML");
    expect(cfg.dashboard,
           "config_io: runtime dashboard should be parsed from TOML");
    expect(!Config{}.dashboard,
           "config_io: the dashboard should be opt-in, so a config that does "
           "not mention it leaves progress output off");
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

void test_config_io_elitism_rate_parsed() {
    const Config cfg = config_from_toml_string(
        "[genetic]\nselection_rate = 0.6\nelitism_rate = 0.2\n");
    expect(cfg.elitism_rate == 0.2,
           "config_io: genetic.elitism_rate should be parsed from TOML");
}

void test_config_io_elitism_not_less_than_selection_throws() {
    bool threw = false;
    try {
        config_from_toml_string(
            "[genetic]\nselection_rate = 0.3\nelitism_rate = 0.3\n");
    } catch (const std::exception& exc) {
        threw = true;
        const std::string msg(exc.what());
        expect(msg.find("elitism_rate") != std::string::npos,
               "config_io: constraint error should name elitism_rate");
    }
    expect(threw,
           "config_io: elitism_rate not less than selection_rate should throw");
}

// Pinned because the default is what an archived config inherits wherever it
// states no scheme. It has moved three times -- WeightedAverage, Nsga2 (renamed
// Nsga2Truncate), and Nsga2Apportion from 2026-08-14 -- and only the 64
// `sweep_B_pop50_takeoff_*` configs under 2026-07-13-fretish-sweeps are
// exposed.
// See "Config vintage" in experiments/README.md before moving it again.
void test_config_io_selection_scheme_defaults_to_nsga2_apportion() {
    const Config cfg = config_from_toml_string("");
    expect(cfg.selection_scheme == SelectionScheme::Nsga2Apportion,
           "config_io: selection_scheme should default to Nsga2Apportion");
}

// Pinned because the default is what every archived config inherits: the key
// did not exist before 2026-08-11, so no archived config can state it, and
// moving this back to Tiered silently re-reads every one of them under a
// different status objective. See "Config vintage" in experiments/README.md.
void test_config_io_status_grading_defaults_to_mrs() {
    const Config cfg = config_from_toml_string("");
    expect(cfg.status_grading == StatusGrading::Mrs,
           "config_io: status_grading should default to Mrs");
}

void test_config_io_status_grading_tiered_parsed() {
    const Config cfg =
        config_from_toml_string("[fitness]\nstatus_grading = \"tiered\"\n");
    expect(cfg.status_grading == StatusGrading::Tiered,
           "config_io: status_grading = \"tiered\" should parse as Tiered");
}

void test_config_io_status_grading_mrs_parsed() {
    const Config cfg =
        config_from_toml_string("[fitness]\nstatus_grading = \"mrs\"\n");
    expect(cfg.status_grading == StatusGrading::Mrs,
           "config_io: status_grading = \"mrs\" should parse as Mrs");
}

void test_config_io_mrs_admission_order_defaults_to_degree() {
    const Config cfg;
    expect(cfg.mrs_admission_order == MrsAdmissionOrder::Degree,
           "config_io: mrs_admission_order should default to Degree");
}

void test_config_io_mrs_admission_order_spec_parsed() {
    const Config cfg =
        config_from_toml_string("[fitness]\nmrs_admission_order = \"spec\"\n");
    expect(cfg.mrs_admission_order == MrsAdmissionOrder::Spec,
           "config_io: mrs_admission_order = \"spec\" should parse as Spec");
}

void test_config_io_mrs_admission_order_degree_parsed() {
    const Config cfg = config_from_toml_string(
        "[fitness]\nmrs_admission_order = \"degree\"\n");
    expect(cfg.mrs_admission_order == MrsAdmissionOrder::Degree,
           "config_io: mrs_admission_order = \"degree\" should parse as "
           "Degree");
}

void test_config_io_mrs_admission_order_rejects_unknown() {
    bool threw = false;
    try {
        config_from_toml_string(
            "[fitness]\nmrs_admission_order = \"rotate\"\n");
    } catch (const std::exception&) {
        threw = true;
    }
    expect(threw,
           "config_io: an unknown mrs_admission_order should be rejected");
}

void test_config_io_status_grading_rejects_unknown() {
    bool threw = false;
    try {
        config_from_toml_string("[fitness]\nstatus_grading = \"greedy\"\n");
    } catch (const std::exception&) {
        threw = true;
    }
    expect(threw, "config_io: an unknown status_grading should be rejected");
}

void test_config_io_selection_scheme_weighted_parsed() {
    const Config cfg =
        config_from_toml_string("[genetic]\nselection_scheme = \"weighted\"\n");
    expect(cfg.selection_scheme == SelectionScheme::WeightedAverage,
           "config_io: selection_scheme = \"weighted\" should parse as "
           "WeightedAverage");
}

void test_config_io_selection_scheme_nsga2_truncate_parsed() {
    const Config cfg = config_from_toml_string(
        "[genetic]\nselection_scheme = \"nsga2-truncate\"\n");
    expect(cfg.selection_scheme == SelectionScheme::Nsga2Truncate,
           "config_io: selection_scheme = \"nsga2-truncate\" should be parsed");
}

void test_config_io_selection_scheme_nsga2_apportion_parsed() {
    const Config cfg = config_from_toml_string(
        "[genetic]\nselection_scheme = \"nsga2-apportion\"\n");
    expect(cfg.selection_scheme == SelectionScheme::Nsga2Apportion,
           "config_io: selection_scheme = \"nsga2-apportion\" should be "
           "parsed");
}

// The two original spellings are rejected rather than aliased, and rejected by
// name: an archived config that sets one must fail loudly and say what to do,
// not run silently under a scheme this binary no longer calls by that name.
void expect_retired_spelling_rejected(const std::string& spelling) {
    bool threw = false;
    try {
        config_from_toml_string("[genetic]\nselection_scheme = \"" + spelling +
                                "\"\n");
    } catch (const std::exception& exc) {
        threw = true;
        const std::string msg(exc.what());
        expect(msg.find(spelling) != std::string::npos,
               "config_io: the error should quote the retired spelling " +
                   spelling);
        expect(msg.find("PROVENANCE.json") != std::string::npos,
               "config_io: the error should say how to reproduce an archived "
               "campaign that sets " +
                   spelling);
    }
    expect(threw, "config_io: the retired spelling " + spelling +
                      " should be rejected, not aliased");
}

void test_config_io_selection_scheme_retired_nsga2_rejected() {
    expect_retired_spelling_rejected("nsga2");
}

void test_config_io_selection_scheme_retired_replicate_rejected() {
    expect_retired_spelling_rejected("nsga2-replicate");
}

void test_config_io_selection_scheme_invalid_throws() {
    bool threw = false;
    try {
        config_from_toml_string("[genetic]\nselection_scheme = \"pareto\"\n");
    } catch (const std::exception& exc) {
        threw = true;
        const std::string msg(exc.what());
        expect(msg.find("selection_scheme") != std::string::npos,
               "config_io: invalid selection_scheme error should name the "
               "field");
    }
    expect(threw, "config_io: an unknown selection_scheme should throw");
}

void test_config_io_similarity_metric_defaults_to_logarithmic() {
    const Config cfg = config_from_toml_string("");
    expect(cfg.similarity_metric == SimilarityMetric::Logarithmic,
           "config_io: similarity_metric should default to Logarithmic");
}

void test_config_io_similarity_metric_direct_parsed() {
    const Config cfg =
        config_from_toml_string("[model_counting]\nmetric = \"direct\"\n");
    expect(cfg.similarity_metric == SimilarityMetric::Direct,
           "config_io: metric = \"direct\" should parse as Direct");
}

void test_config_io_similarity_metric_logarithmic_parsed() {
    const Config cfg =
        config_from_toml_string("[model_counting]\nmetric = \"logarithmic\"\n");
    expect(cfg.similarity_metric == SimilarityMetric::Logarithmic,
           "config_io: metric = \"logarithmic\" should parse as Logarithmic");
}

void test_config_io_similarity_metric_invalid_throws() {
    bool threw = false;
    try {
        config_from_toml_string("[model_counting]\nmetric = \"geometric\"\n");
    } catch (const std::exception& exc) {
        threw = true;
        const std::string msg(exc.what());
        expect(msg.find("metric") != std::string::npos,
               "config_io: invalid metric error should name the field");
    }
    expect(threw, "config_io: an unknown similarity metric should throw");
}

void test_config_io_empty_string_gives_defaults() {
    const Config cfg = config_from_toml_string("");
    const Config defaults;
    expect(cfg.generations == defaults.generations,
           "config_io: empty TOML should give default generations");
    expect(cfg.population_size == defaults.population_size,
           "config_io: empty TOML should give default population_size");
}

void test_config_io_repair_mode_defaults_to_monolithic() {
    const Config cfg = config_from_toml_string("");
    expect(cfg.repair_mode == RepairMode::Monolithic,
           "config_io: repair_mode should default to Monolithic");
}

void test_config_io_repair_mode_muc_parsed() {
    const Config cfg =
        config_from_toml_string("[tlsf]\nrepair_mode = \"muc\"\n");
    expect(cfg.repair_mode == RepairMode::Muc,
           "config_io: repair_mode = \"muc\" should be parsed");
}

void test_config_io_repair_mode_monolithic_parsed() {
    const Config cfg =
        config_from_toml_string("[tlsf]\nrepair_mode = \"monolithic\"\n");
    expect(cfg.repair_mode == RepairMode::Monolithic,
           "config_io: repair_mode = \"monolithic\" should be parsed");
}

void test_config_io_repair_mode_invalid_throws() {
    bool threw = false;
    try {
        config_from_toml_string("[tlsf]\nrepair_mode = \"iterative\"\n");
    } catch (const std::exception& exc) {
        threw = true;
        const std::string msg(exc.what());
        expect(msg.find("repair_mode") != std::string::npos,
               "config_io: invalid repair_mode error should name the field");
    }
    expect(threw, "config_io: an unknown repair_mode should throw");
}

void test_config_io_muc_max_iterations_parsed() {
    const Config cfg =
        config_from_toml_string("[tlsf]\nmuc_max_iterations = 7\n");
    expect(cfg.muc_max_iterations == 7,
           "config_io: tlsf.muc_max_iterations should be parsed");
}

void test_config_io_muc_max_iterations_nonpositive_throws() {
    bool threw = false;
    try {
        config_from_toml_string("[tlsf]\nmuc_max_iterations = 0\n");
    } catch (const std::exception&) {
        threw = true;
    }
    expect(threw, "config_io: muc_max_iterations = 0 should throw");
}

// Every key config_key_spec() declares. Fails if an apply_* function gains a
// key the unknown-key spec was not told about. A key absent from this TOML is
// not covered, so a new key belongs here as well as in the two places
// config_io.cpp names.
void test_config_io_known_keys_do_not_warn() {
    const std::string toml = R"(
[genetic]
generations      = 5
population_size  = 100
selection_rate   = 0.5
elitism_rate     = 0.1
crossover_rate   = 0.2
mutation_rate    = 0.8
selection_scheme = "nsga2-truncate"

[fitness]
weight_syntactic = 0.3
weight_semantic  = 0.4
weight_status    = 0.6

[mutation]
p_trigger                = 0.3
p_response               = 0.7
p_timing                 = 0.1
p_add_assumption         = 0.05
p_conditional_assumption = 0.25
strengthen_assumptions   = true
allow_output_assumptions = false

[tlsf]
repair_mode        = "muc"
muc_max_iterations = 32

[tlsf.mutation]
p_assumption = 0.3
p_temporal   = 0.2

[model_counting]
default_bound = 10
metric        = "direct"

[filters]
run_weakening       = false
run_implication     = false
run_vacuity         = true
run_well_separation = true

[runtime]
black_timeout_ms             = 500
ltlsynt_timeout_ms           = 30000
ltl2tgba_timeout_ms          = 1000
ltlfilt_timeout_ms           = 2000
ganak_timeout_ms             = 4000
parallel                     = 4
max_concurrent_realizability = 6
max_scoring_failure_rate     = 0.05
dashboard                    = true
)";
    const std::string warnings = warnings_from(toml);
    expect(warnings.empty(),
           "config_io: a config of known keys should warn about none, got: " +
               warnings);
}

void test_config_io_unknown_section_warns() {
    const std::string warnings = warnings_from("[genetics]\ngenerations = 5\n");
    expect(warnings.find("unknown section [genetics]") != std::string::npos,
           "config_io: an unknown section should be named in a warning");
}

void test_config_io_unknown_key_warns() {
    const std::string warnings = warnings_from("[genetic]\ngenerationss = 5\n");
    expect(
        warnings.find("unknown key genetic.generationss") != std::string::npos,
        "config_io: an unknown key should be warned about by its full path");
}

void test_config_io_unknown_top_level_key_warns() {
    const std::string warnings = warnings_from("generations = 5\n");
    expect(warnings.find("unknown key generations") != std::string::npos,
           "config_io: a key outside any section should be warned about");
}

void test_config_io_unknown_nested_key_warns() {
    const std::string warnings = warnings_from(
        "[tlsf.mutation]\np_assumption_ = 0.3\np_temporal_ = 0.2\n");
    expect(warnings.find("unknown key tlsf.mutation.p_assumption_") !=
               std::string::npos,
           "config_io: an unknown key in a nested section should be warned "
           "about by its full path");
    expect(warnings.find("unknown key tlsf.mutation.p_temporal_") !=
               std::string::npos,
           "config_io: a second unknown key in [tlsf.mutation] should also be "
           "warned about by its full path");
}

void test_config_io_unknown_nested_section_warns() {
    const std::string warnings =
        warnings_from("[filters.interval]\ndedup = 2\n");
    expect(warnings.find("unknown section [filters.interval]") !=
               std::string::npos,
           "config_io: an unknown nested section should be warned about by its "
           "full path");
}

// A key in the wrong section is the typo the per-section spec exists to catch.
void test_config_io_misplaced_key_warns() {
    const std::string warnings = warnings_from("[fitness]\ngenerations = 5\n");
    expect(
        warnings.find("unknown key fitness.generations") != std::string::npos,
        "config_io: a known key in the wrong section should be warned "
        "about");
}

void test_config_io_unknown_key_still_applies_known_ones() {
    std::string warnings;
    const Config cfg = config_capturing_warnings(
        "[genetic]\ngenerations = 5\nnonsense = 1\n", warnings);
    expect(cfg.generations == 5,
           "config_io: an unknown key should not stop known keys applying");
}

}  // namespace

void run_config_io_tests() {
    test_config_io_all_fields();
    test_config_io_partial_overrides_defaults();
    test_config_io_missing_file_throws();
    test_config_io_invalid_toml_throws();
    test_config_io_out_of_range_probability_throws();
    test_config_io_elitism_rate_parsed();
    test_config_io_elitism_not_less_than_selection_throws();
    test_config_io_selection_scheme_defaults_to_nsga2_apportion();
    test_config_io_status_grading_defaults_to_mrs();
    test_config_io_status_grading_tiered_parsed();
    test_config_io_status_grading_mrs_parsed();
    test_config_io_status_grading_rejects_unknown();
    test_config_io_mrs_admission_order_defaults_to_degree();
    test_config_io_mrs_admission_order_spec_parsed();
    test_config_io_mrs_admission_order_degree_parsed();
    test_config_io_mrs_admission_order_rejects_unknown();
    test_config_io_selection_scheme_weighted_parsed();
    test_config_io_selection_scheme_nsga2_truncate_parsed();
    test_config_io_selection_scheme_nsga2_apportion_parsed();
    test_config_io_selection_scheme_retired_nsga2_rejected();
    test_config_io_selection_scheme_retired_replicate_rejected();
    test_config_io_selection_scheme_invalid_throws();
    test_config_io_similarity_metric_defaults_to_logarithmic();
    test_config_io_similarity_metric_direct_parsed();
    test_config_io_similarity_metric_logarithmic_parsed();
    test_config_io_similarity_metric_invalid_throws();
    test_config_io_empty_string_gives_defaults();
    test_config_io_repair_mode_defaults_to_monolithic();
    test_config_io_repair_mode_muc_parsed();
    test_config_io_repair_mode_monolithic_parsed();
    test_config_io_repair_mode_invalid_throws();
    test_config_io_muc_max_iterations_parsed();
    test_config_io_muc_max_iterations_nonpositive_throws();
    test_config_io_known_keys_do_not_warn();
    test_config_io_unknown_section_warns();
    test_config_io_unknown_key_warns();
    test_config_io_unknown_top_level_key_warns();
    test_config_io_unknown_nested_key_warns();
    test_config_io_unknown_nested_section_warns();
    test_config_io_misplaced_key_warns();
    test_config_io_unknown_key_still_applies_known_ones();
}
