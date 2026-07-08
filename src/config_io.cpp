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
    "model_counting", "filters", "runtime"};

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
    if (auto val = tbl["crossover_rate"].value<double>()) {
        require_probability(*val, "genetic.crossover_rate");
        cfg.crossover_rate = *val;
    }
    if (auto val = tbl["mutation_rate"].value<double>()) {
        require_probability(*val, "genetic.mutation_rate");
        cfg.mutation_rate = *val;
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
}

void apply_model_counting(const toml::table& tbl, Config& cfg) {
    if (auto val = tbl["default_bound"].value<int64_t>()) {
        cfg.default_model_counting_bound = static_cast<std::size_t>(
            require_positive(*val, "model_counting.default_bound"));
    }
}

void apply_filters(const toml::table& tbl, Config& cfg) {
    if (auto val = tbl["run_weakening"].value<bool>()) {
        cfg.run_weakening_filter = *val;
    }
    if (auto val = tbl["run_implication"].value<bool>()) {
        cfg.run_implication_filter = *val;
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
    if (auto val = tbl["parallel"].value<int64_t>()) {
        if (*val <= 0) {
            throw std::runtime_error("config: runtime.parallel must be >= 1");
        }
        cfg.parallel = static_cast<std::size_t>(*val);
    }
    if (auto val = tbl["report_cpu_timing"].value<bool>()) {
        cfg.report_cpu_timing = *val;
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
