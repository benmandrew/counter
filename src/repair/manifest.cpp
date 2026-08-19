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
#include "filter/well_separation.hpp"
#include "fitness/function.hpp"
#include "genetic/accumulator.hpp"
#include "runner/black.hpp"
#include "runner/ganak.hpp"
#include "runner/ltlfilt.hpp"
#include "runner/spot.hpp"
#include "version.hpp"

namespace {

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
// 5 added budget_screen, so a zero-yield run can be told apart from one whose
// ltlsynt budget could not decide its own input specification.
// 6 added fitness.status_grading, since the status objective now has two
// scales and a manifest that does not name the one in use cannot be read.
// 7 dropped fitness.weight_halstead along with the objective it weighted, so
// the objective vector is one shorter and a reader keying on its position
// against an older manifest reads the wrong component.
// 8 added mutation.p_remove_guarantee. The operator can delete a guarantee, so
// a repair may hold fewer requirements than the specification it came from, and
// a reader comparing counts needs to know whether the run could do that.
// 9 added n_spot_decided and n_escalations_declined, and redefined
// tool_calls.black.calls as black's own exec count rather than the
// satisfiability cache-miss count, which SPOT taking the first stage split in
// two. Comparing black.calls across the boundary compares different things.
// 10 added n_well_separation_errors. The well-separation query used to let an
// ltlsynt error end the run, so a run that finished implies zero of them; from
// this version a finished run may have dropped candidates on a non-answer, and
// only this field says how many.
// 11 added input_screen_error. Before it, a screen that raised ended the run
// before any manifest was written, so `input_screen: null` meant the input
// passed every check. From this version the run survives, and null means either
// that or a screen that never produced a verdict -- this field is what tells
// them apart.
// 12 added fitness.mrs_admission_order. The MRS walk's score depends on the
// order it admits parts in, so two runs on one specification under one grading
// scale are not comparable without it.
// 13 added genetic.accumulate_repairs and n_accumulated_repairs. Before it a
// run's repairs all came from its final population, so the output was a
// function of the last generation alone; from this version the key can union
// in the repairs earlier generations found, and only this field says how many
// of them the run would otherwise have thrown away.
// 14 added n_ltlsynt_capability_errors. Before it, an ltlsynt run that reported
// a limit of its own instead of a verdict ended the run and no manifest was
// written at all; from this version the query resolves as undecided and this
// field is the only record that it happened.
constexpr int k_schema_version = 14;

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

const char* mrs_admission_order_name(MrsAdmissionOrder order) {
    switch (order) {
        case MrsAdmissionOrder::Spec:
            return "spec";
        case MrsAdmissionOrder::Degree:
            return "degree";
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
          {"selection_scheme", scheme_name(cfg.selection_scheme)},
          {"accumulate_repairs", cfg.accumulate_repairs}}},
        {"fitness",
         {{"weight_syntactic", cfg.fitness_weight_syntactic},
          {"weight_semantic", cfg.fitness_weight_semantic},
          {"weight_status", cfg.fitness_weight_status},
          {"status_grading", status_grading_name(cfg.status_grading)},
          {"mrs_admission_order",
           mrs_admission_order_name(cfg.mrs_admission_order)}}},
        {"mutation",
         {{"p_trigger", cfg.p_trigger},
          {"p_response", cfg.p_response},
          {"p_timing", cfg.p_timing},
          {"p_add_assumption", cfg.p_add_assumption},
          {"p_remove_guarantee", cfg.p_remove_guarantee},
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
            // "calls" is black's own exec count, which parted company with the
            // cache-miss count when SPOT took the first stage: a miss now
            // reaches black only if SPOT left it undecided and its polarity
            // says black could still answer.
            {"black", row(SatisfiabilityChecker::n_black_calls,
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
        // Null unless a tool raised while screening, in which case
        // `input_screen` above is null for want of a verdict rather than
        // because the input passed. A campaign partitioning on `input_screen`
        // must read this key too, or it counts an unscreened run as a clean
        // one.
        {"input_screen_error", InputScreen::error.empty()
                                   ? nlohmann::json(nullptr)
                                   : nlohmann::json(InputScreen::error)},
        // Null when no screen ran, which is the case under an unlimited
        // budget. `decided` false means the budget could not settle the input
        // specification's own realizability, so a campaign can partition its
        // zero-yield rows on the budget rather than read them as search
        // failures. `observed_ms` is a lower bound when undecided: the child
        // was killed at the budget.
        {"budget_screen",
         BudgetScreen::observed_ms < 0
             ? nlohmann::json(nullptr)
             : nlohmann::json({{"ltlsynt_ms", BudgetScreen::observed_ms},
                               {"decided", BudgetScreen::decided}})},
        {"config", config_json(cfg)},
        {"tool_calls", tool_calls_json()},
        {"n_constant_folded", SatisfiabilityChecker::n_constant_folded.load()},
        {"n_spot_decided", SatisfiabilityChecker::n_spot_decided.load()},
        {"n_escalations_declined",
         SatisfiabilityChecker::n_escalations_declined.load()},
        {"n_weak_operator_unresolved",
         SatisfiabilityChecker::n_weak_operator_unresolved.load()},
        {"n_tautology_substitutions", Ltl2tgbaStats::n_tautology_substitutions},
        // Repairs the cross-generation accumulator added that the final
        // population's own collection did not already hold. Zero when
        // genetic.accumulate_repairs is off, and also when every accumulated
        // repair happened to survive to the last generation, so it is read
        // against the config key rather than on its own.
        {"n_accumulated_repairs", AccumulatorStats::n_contributed},
        // ltlsynt calls that reported SPOT's acceptance-set ceiling rather than
        // a verdict, resolved as undecided rather than ending the run.
        {"n_ltlsynt_capability_errors",
         RealizabilityChecker::n_capability_errors},
        // Well-separation queries that raised instead of answering, resolved as
        // undecided (the candidate is dropped) rather than propagating.
        {"n_well_separation_errors", WellSeparationStats::n_errors.load()},
        {"fitness_cache", fitness_cache_json()}};

    const std::filesystem::path path = dir / k_run_manifest_name;
    std::ofstream out(path);
    if (!out) {
        std::cerr << "warning: could not write " << path.string()
                  << "; the run's repairs are unaffected\n";
        return;
    }
    out << manifest.dump(2) << "\n";
}
