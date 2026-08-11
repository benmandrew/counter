#include "manifest.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "filter/correctness.hpp"
#include "fitness/function.hpp"
#include "runner/black.hpp"
#include "runner/ganak.hpp"
#include "runner/ltlfilt.hpp"
#include "runner/spot.hpp"
#include "version.hpp"

namespace {

constexpr const char* k_manifest_filename = "run.json";

// Bumped when a key changes meaning or leaves, so a reader can tell which
// shape it is holding rather than guessing from which keys are present.
// 2 added fitness_cache, n_constant_folded and n_tautology_substitutions, when
// those counters stopped printing to stdout by default and this became their
// only unconditional record. Anything print_diagnostics_report shows has to
// appear here too, or turning the flag off loses it.
// 3 added input_screen, so a campaign can partition its runs on whether the
// input itself failed a correctness check rather than grepping the log for the
// warning.
// 4 added n_weak_operator_unresolved. It counts satisfiability queries
// abandoned rather than answered, so a run that reports repairs with a
// non-zero count screened less than a run reporting the same repairs with
// zero -- which is not visible from any other field.
constexpr int k_schema_version = 5;

// The inverse of the spellings config_io.cpp parses. It has no table to
// borrow -- it only ever goes string to enum -- so these must be kept in step
// with it by hand. Getting one wrong writes a manifest that no longer round
// trips into the config it describes.
const char* scheme_name(SelectionScheme scheme) {
    switch (scheme) {
        case SelectionScheme::WeightedAverage:
            return "weighted";
        case SelectionScheme::Nsga2Truncate:
            return "nsga2-truncate";
        case SelectionScheme::Nsga2Apportion:
            return "nsga2-apportion";
    }
    return "unknown";
}

const char* metric_name(SimilarityMetric metric) {
    switch (metric) {
        case SimilarityMetric::Direct:
            return "direct";
        case SimilarityMetric::Logarithmic:
            return "logarithmic";
    }
    return "unknown";
}

const char* status_grading_name(StatusGrading grading) {
    switch (grading) {
        case StatusGrading::Tiered:
            return "tiered";
        case StatusGrading::Mrs:
            return "mrs";
    }
    return "unknown";
}

const char* repair_mode_name(RepairMode mode) {
    switch (mode) {
        case RepairMode::Monolithic:
            return "monolithic";
        case RepairMode::Muc:
            return "muc";
    }
    return "unknown";
}

std::string utc_timestamp() {
    const std::time_t now =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm parts{};
    if (gmtime_r(&now, &parts) == nullptr) {
        return "";
    }
    std::array<char, 32> buffer{};
    if (std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%SZ",
                      &parts) == 0) {
        return "";
    }
    return {buffer.data()};
}

// Counts distinct repair indices rather than files: the FRETISH path writes
// one repair_N.json per repair and the TLSF path writes repair_N.tlsf beside a
// repair_N.fitness.json, so counting files would double the TLSF total.
std::size_t count_repairs(const std::filesystem::path& dir) {
    std::error_code error;
    std::set<std::string> indices;
    for (const auto& entry : std::filesystem::directory_iterator(dir, error)) {
        const std::string name = entry.path().filename().string();
        if (name.rfind("repair_", 0) != 0) {
            continue;
        }
        const std::size_t dot = name.find('.');
        if (dot == std::string::npos) {
            continue;
        }
        indices.insert(name.substr(0, dot));
    }
    return indices.size();
}

nlohmann::json config_json(const Config& cfg) {
    // Mirrors the TOML section layout so a manifest diffs directly against a
    // config file rather than needing a key-by-key translation.
    return {
        {"genetic",
         {{"generations", cfg.generations},
          {"population_size", cfg.population_size},
          {"selection_rate", cfg.selection_rate},
          {"elitism_rate", cfg.elitism_rate},
          {"crossover_rate", cfg.crossover_rate},
          {"mutation_rate", cfg.mutation_rate},
          {"selection_scheme", scheme_name(cfg.selection_scheme)}}},
        {"fitness",
         {{"weight_syntactic", cfg.fitness_weight_syntactic},
          {"weight_semantic", cfg.fitness_weight_semantic},
          {"weight_halstead", cfg.fitness_weight_halstead},
          {"weight_status", cfg.fitness_weight_status},
          {"status_grading", status_grading_name(cfg.status_grading)}}},
        {"mutation",
         {{"p_trigger", cfg.p_trigger},
          {"p_response", cfg.p_response},
          {"p_timing", cfg.p_timing},
          {"p_add_assumption", cfg.p_add_assumption},
          {"p_conditional_assumption", cfg.p_conditional_assumption},
          {"strengthen_assumptions", cfg.strengthen_assumptions},
          {"allow_output_assumptions", cfg.allow_output_assumptions}}},
        {"tlsf",
         {{"repair_mode", repair_mode_name(cfg.repair_mode)},
          {"muc_max_iterations", cfg.muc_max_iterations},
          {"mutation",
           {{"p_assumption", cfg.tlsf_p_assumption},
            {"p_temporal", cfg.tlsf_p_temporal}}}}},
        {"model_counting",
         {{"default_bound", cfg.default_model_counting_bound},
          {"metric", metric_name(cfg.similarity_metric)}}},
        {"filters",
         {{"run_weakening", cfg.run_weakening_filter},
          {"run_implication", cfg.run_implication_filter},
          {"run_vacuity", cfg.run_vacuity_filter},
          {"run_well_separation", cfg.run_well_separation_filter}}},
        {"runtime",
         {{"black_timeout_ms", cfg.black_timeout.count()},
          {"ltlsynt_timeout_ms", cfg.ltlsynt_timeout.count()},
          {"ltl2tgba_timeout_ms", cfg.ltl2tgba_timeout.count()},
          {"ltlfilt_timeout_ms", cfg.ltlfilt_timeout.count()},
          {"ganak_timeout_ms", cfg.ganak_timeout.count()},
          {"parallel", cfg.parallel},
          {"max_concurrent_realizability", cfg.max_concurrent_realizability},
          {"max_scoring_failure_rate", cfg.max_scoring_failure_rate},
          {"dashboard", cfg.dashboard}}}};
}

nlohmann::json tool_calls_json() {
    auto row = [](std::size_t calls, std::size_t cache_hits,
                  std::size_t timeouts, double total_s) {
        return nlohmann::json{{"calls", calls},
                              {"cache_hits", cache_hits},
                              {"timeouts", timeouts},
                              {"total_s", total_s}};
    };
    return {{"ltl2tgba",
             row(Ltl2tgbaStats::n_cache_misses, Ltl2tgbaStats::n_cache_hits,
                 Ltl2tgbaStats::n_timeouts, Ltl2tgbaStats::total_time_s)},
            {"ltlfilt",
             row(LtlfiltStats::n_cache_misses, LtlfiltStats::n_cache_hits,
                 LtlfiltStats::n_timeouts, LtlfiltStats::total_time_s)},
            {"ltlsynt", row(RealizabilityChecker::n_cache_misses,
                            RealizabilityChecker::n_cache_hits,
                            RealizabilityChecker::n_timeouts,
                            RealizabilityChecker::total_time_s)},
            {"black", row(SatisfiabilityChecker::n_cache_misses,
                          SatisfiabilityChecker::n_cache_hits,
                          SatisfiabilityChecker::n_timeouts,
                          SatisfiabilityChecker::total_time_s)},
            {"ganak", row(GanakStats::n_cache_misses, GanakStats::n_cache_hits,
                          GanakStats::n_timeouts, GanakStats::total_time_s)}};
}

nlohmann::json fitness_cache_json() {
    return {{"hits", AggregateWeightedFitnessFunction::n_cache_hits},
            {"misses", AggregateWeightedFitnessFunction::n_cache_misses}};
}

}  // namespace

void write_run_manifest(const std::string& output_dir,
                        const std::string& input_path, std::size_t seed,
                        const Config& cfg, double wall_s) {
    const std::filesystem::path dir(output_dir);
    const nlohmann::json manifest{
        {"schema_version", k_schema_version},
        {"tool", "counter"},
        {"commit", version::commit()},
        {"commit_short", version::commit_short()},
        {"dirty", version::dirty()},
        {"finished_utc", utc_timestamp()},
        {"wall_s", wall_s},
        {"input", input_path},
        {"seed", seed},
        {"n_repairs", count_repairs(dir)},
        // Null when the input passed every check, which is the ordinary case;
        // the name of the check it failed otherwise.
        {"input_screen", InputScreen::failed_check.empty()
                             ? nlohmann::json(nullptr)
                             : nlohmann::json(InputScreen::failed_check)},
        {"config", config_json(cfg)},
        {"tool_calls", tool_calls_json()},
        {"n_constant_folded", SatisfiabilityChecker::n_constant_folded.load()},
        {"n_weak_operator_unresolved",
         SatisfiabilityChecker::n_weak_operator_unresolved.load()},
        {"n_tautology_substitutions", Ltl2tgbaStats::n_tautology_substitutions},
        {"fitness_cache", fitness_cache_json()}};

    const std::filesystem::path path = dir / k_manifest_filename;
    std::ofstream out(path);
    if (!out) {
        std::cerr << "warning: could not write " << path.string()
                  << "; the run's repairs are unaffected\n";
        return;
    }
    out << manifest.dump(2) << "\n";
}
