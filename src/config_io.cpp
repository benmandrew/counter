#include "config_io.hpp"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

#define TOML_EXCEPTIONS 1
#include <toml++/toml.hpp>

namespace {

template <typename T>
T require_positive(T value, const char* name) {
    if (value <= T{0}) {
        throw std::runtime_error(std::string("config: ") + name +
                                 " must be positive");
    }
    return value;
}

void require_probability(double value, const char* name) {
    if (value < 0.0 || value > 1.0) {
        throw std::runtime_error(std::string("config: ") + name +
                                 " must be in [0, 1]");
    }
}

void require_nonnegative(double value, const char* name) {
    if (value < 0.0) {
        throw std::runtime_error(std::string("config: ") + name +
                                 " must be >= 0");
    }
}

const std::set<std::string> k_known_sections = {
    "genetic",        "fitness", "syntactic", "mutation",
    "model_counting", "filters", "runtime",   "tlsf"};

void warn_unknown_keys(const toml::table& tbl) {
    for (const auto& [key, node] : tbl) {
        if (node.is_table()) {
            const bool unknown = k_known_sections.find(std::string(key)) ==
                                 k_known_sections.end();
            if (unknown) {
                std::cerr << "config: unknown section [" << key
                          << "], ignoring\n";
            }
        }
    }
}

void apply_genetic(const toml::table& tbl, Config& cfg) {
    if (auto val = tbl["generations"].value<int64_t>()) {
        cfg.generations = static_cast<std::size_t>(
            require_positive(*val, "genetic.generations"));
    }
    if (auto val = tbl["population_size"].value<int64_t>()) {
        cfg.population_size = static_cast<std::size_t>(
            require_positive(*val, "genetic.population_size"));
    }
    if (auto val = tbl["selection_rate"].value<double>()) {
        require_probability(*val, "genetic.selection_rate");
        cfg.selection_rate = *val;
    }
    if (auto val = tbl["elitism_rate"].value<double>()) {
        require_probability(*val, "genetic.elitism_rate");
        cfg.elitism_rate = *val;
    }
    if (auto val = tbl["crossover_rate"].value<double>()) {
        require_probability(*val, "genetic.crossover_rate");
        cfg.crossover_rate = *val;
    }
    if (auto val = tbl["mutation_rate"].value<double>()) {
        require_probability(*val, "genetic.mutation_rate");
        cfg.mutation_rate = *val;
    }
    if (auto val = tbl["selection_scheme"].value<std::string>()) {
        if (*val == "weighted") {
            cfg.selection_scheme = SelectionScheme::WeightedAverage;
        } else if (*val == "nsga2") {
            cfg.selection_scheme = SelectionScheme::Nsga2;
        } else {
            throw std::runtime_error(
                "config: genetic.selection_scheme must be \"weighted\" or "
                "\"nsga2\"");
        }
    }
    // Elites are a subset of the selected parents, so elitism must be strictly
    // smaller than selection. Checked against the final values (either may come
    // from the TOML or fall back to its default).
    if (cfg.elitism_rate >= cfg.selection_rate) {
        throw std::runtime_error(
            "config: genetic.elitism_rate must be less than "
            "genetic.selection_rate");
    }
}

void apply_fitness(const toml::table& tbl, Config& cfg) {
    if (auto val = tbl["weight_syntactic"].value<double>()) {
        require_nonnegative(*val, "fitness.weight_syntactic");
        cfg.fitness_weight_syntactic = *val;
    }
    if (auto val = tbl["weight_semantic"].value<double>()) {
        require_nonnegative(*val, "fitness.weight_semantic");
        cfg.fitness_weight_semantic = *val;
    }
    if (auto val = tbl["weight_halstead"].value<double>()) {
        require_nonnegative(*val, "fitness.weight_halstead");
        cfg.fitness_weight_halstead = *val;
    }
    if (auto val = tbl["weight_status"].value<double>()) {
        require_nonnegative(*val, "fitness.weight_status");
        cfg.fitness_weight_status = *val;
    }
}

void apply_syntactic(const toml::table& tbl, Config& cfg) {
    if (auto val = tbl["weight_trigger"].value<double>()) {
        require_nonnegative(*val, "syntactic.weight_trigger");
        cfg.syntactic_weight_trigger = *val;
    }
    if (auto val = tbl["weight_response"].value<double>()) {
        require_nonnegative(*val, "syntactic.weight_response");
        cfg.syntactic_weight_response = *val;
    }
    if (auto val = tbl["weight_timing"].value<double>()) {
        require_nonnegative(*val, "syntactic.weight_timing");
        cfg.syntactic_weight_timing = *val;
    }
}

void apply_mutation(const toml::table& tbl, Config& cfg) {
    if (auto val = tbl["p_trigger"].value<double>()) {
        require_probability(*val, "mutation.p_trigger");
        cfg.p_trigger = *val;
    }
    if (auto val = tbl["p_response"].value<double>()) {
        require_probability(*val, "mutation.p_response");
        cfg.p_response = *val;
    }
    if (auto val = tbl["p_timing"].value<double>()) {
        require_probability(*val, "mutation.p_timing");
        cfg.p_timing = *val;
    }
    if (auto val = tbl["p_add_assumption"].value<double>()) {
        require_probability(*val, "mutation.p_add_assumption");
        cfg.p_add_assumption = *val;
    }
    if (auto val = tbl["p_conditional_assumption"].value<double>()) {
        require_probability(*val, "mutation.p_conditional_assumption");
        cfg.p_conditional_assumption = *val;
    }
    if (auto val = tbl["strengthen_assumptions"].value<bool>()) {
        cfg.strengthen_assumptions = *val;
    }
    if (auto val = tbl["allow_output_assumptions"].value<bool>()) {
        cfg.allow_output_assumptions = *val;
    }
}

void apply_tlsf(const toml::table& tbl, Config& cfg) {
    if (const auto* mutation = tbl["mutation"].as_table()) {
        if (auto val = (*mutation)["p_assumption"].value<double>()) {
            require_probability(*val, "tlsf.mutation.p_assumption");
            cfg.tlsf_p_assumption = *val;
        }
        if (auto val = (*mutation)["p_guarantee"].value<double>()) {
            require_probability(*val, "tlsf.mutation.p_guarantee");
            cfg.tlsf_p_guarantee = *val;
        }
        if (auto val = (*mutation)["p_temporal"].value<double>()) {
            require_probability(*val, "tlsf.mutation.p_temporal");
            cfg.tlsf_p_temporal = *val;
        }
    }
    if (auto val = tbl["repair_mode"].value<std::string>()) {
        if (*val == "monolithic") {
            cfg.repair_mode = RepairMode::Monolithic;
        } else if (*val == "muc") {
            cfg.repair_mode = RepairMode::Muc;
        } else {
            throw std::runtime_error(
                R"(config: tlsf.repair_mode must be "monolithic" or "muc")");
        }
    }
    if (auto val = tbl["muc_max_iterations"].value<int64_t>()) {
        cfg.muc_max_iterations = static_cast<std::size_t>(
            require_positive(*val, "tlsf.muc_max_iterations"));
    }
}

void apply_model_counting(const toml::table& tbl, Config& cfg) {
    if (auto val = tbl["default_bound"].value<int64_t>()) {
        cfg.default_model_counting_bound = static_cast<std::size_t>(
            require_positive(*val, "model_counting.default_bound"));
    }
    if (auto val = tbl["metric"].value<std::string>()) {
        if (*val == "direct") {
            cfg.similarity_metric = SimilarityMetric::Direct;
        } else if (*val == "logarithmic") {
            cfg.similarity_metric = SimilarityMetric::Logarithmic;
        } else {
            throw std::runtime_error(
                "config: model_counting.metric must be \"direct\" or "
                "\"logarithmic\"");
        }
    }
}

void apply_filters(const toml::table& tbl, Config& cfg) {
    if (auto val = tbl["run_weakening"].value<bool>()) {
        cfg.run_weakening_filter = *val;
    }
    if (auto val = tbl["run_implication"].value<bool>()) {
        cfg.run_implication_filter = *val;
    }
    if (auto val = tbl["run_vacuity"].value<bool>()) {
        cfg.run_vacuity_filter = *val;
    }
    if (auto val = tbl["run_well_separation"].value<bool>()) {
        cfg.run_well_separation_filter = *val;
    }
    if (const auto* intervals = tbl["intervals"].as_table()) {
        const auto parse_interval = [&](const char* key, const char* name,
                                        std::size_t& field) {
            if (auto val = (*intervals)[key].value<int64_t>()) {
                field = static_cast<std::size_t>(require_positive(*val, name));
            }
        };
        parse_interval("dedup", "filters.intervals.dedup",
                       cfg.dedup_filter_interval);
        parse_interval("false_condition", "filters.intervals.false_condition",
                       cfg.false_condition_filter_interval);
        parse_interval("weakening", "filters.intervals.weakening",
                       cfg.weakening_filter_interval);
        parse_interval("bloat", "filters.intervals.bloat",
                       cfg.bloat_filter_interval);
        parse_interval("vacuity", "filters.intervals.vacuity",
                       cfg.vacuity_filter_interval);
        parse_interval("well_separation", "filters.intervals.well_separation",
                       cfg.well_separation_filter_interval);
    }
}

void apply_runtime(const toml::table& tbl, Config& cfg) {
    if (auto val = tbl["black_timeout_ms"].value<int64_t>()) {
        if (*val < 0) {
            throw std::runtime_error(
                "config: runtime.black_timeout_ms must be >= 0");
        }
        cfg.black_timeout = std::chrono::milliseconds{*val};
    }
    if (auto val = tbl["ltlsynt_timeout_ms"].value<int64_t>()) {
        if (*val < 0) {
            throw std::runtime_error(
                "config: runtime.ltlsynt_timeout_ms must be >= 0");
        }
        cfg.ltlsynt_timeout = std::chrono::milliseconds{*val};
    }
    if (auto val = tbl["ltl2tgba_timeout_ms"].value<int64_t>()) {
        if (*val < 0) {
            throw std::runtime_error(
                "config: runtime.ltl2tgba_timeout_ms must be >= 0");
        }
        cfg.ltl2tgba_timeout = std::chrono::milliseconds{*val};
    }
    if (auto val = tbl["parallel"].value<int64_t>()) {
        if (*val <= 0) {
            throw std::runtime_error("config: runtime.parallel must be >= 1");
        }
        cfg.parallel = static_cast<std::size_t>(*val);
    }
    if (auto val = tbl["max_concurrent_realizability"].value<int64_t>()) {
        if (*val < 0) {
            throw std::runtime_error(
                "config: runtime.max_concurrent_realizability must be >= 0");
        }
        cfg.max_concurrent_realizability = static_cast<std::size_t>(*val);
    }
    if (auto val = tbl["report_cpu_timing"].value<bool>()) {
        cfg.report_cpu_timing = *val;
    }
    if (auto val = tbl["max_scoring_failure_rate"].value<double>()) {
        require_probability(*val, "runtime.max_scoring_failure_rate");
        cfg.max_scoring_failure_rate = *val;
    }
}

Config apply_toml(const toml::table& tbl) {
    warn_unknown_keys(tbl);
    Config cfg;
    if (const auto* sec = tbl["genetic"].as_table()) {
        apply_genetic(*sec, cfg);
    }
    if (const auto* sec = tbl["fitness"].as_table()) {
        apply_fitness(*sec, cfg);
    }
    if (const auto* sec = tbl["syntactic"].as_table()) {
        apply_syntactic(*sec, cfg);
    }
    if (const auto* sec = tbl["mutation"].as_table()) {
        apply_mutation(*sec, cfg);
    }
    if (const auto* sec = tbl["model_counting"].as_table()) {
        apply_model_counting(*sec, cfg);
    }
    if (const auto* sec = tbl["filters"].as_table()) {
        apply_filters(*sec, cfg);
    }
    if (const auto* sec = tbl["runtime"].as_table()) {
        apply_runtime(*sec, cfg);
    }
    if (const auto* sec = tbl["tlsf"].as_table()) {
        apply_tlsf(*sec, cfg);
    }
    return cfg;
}

}  // namespace

Config config_from_toml(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("config: file does not exist: " +
                                 path.string());
    }
    toml::table tbl;
    try {
        tbl = toml::parse_file(path.string());
    } catch (const toml::parse_error& exc) {
        throw std::runtime_error(std::string("config: TOML parse error: ") +
                                 exc.what());
    }
    return apply_toml(tbl);
}

Config config_from_toml_string(const std::string& content) {
    toml::table tbl;
    try {
        tbl = toml::parse(content);
    } catch (const toml::parse_error& exc) {
        throw std::runtime_error(std::string("config: TOML parse error: ") +
                                 exc.what());
    }
    return apply_toml(tbl);
}
