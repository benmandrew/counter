#pragma once

/// @file serialisation.hpp
/// @brief JSON serialisation and deserialisation for Timing, Requirement,
///        Specification, and related types via nlohmann/json.

#include <cstddef>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <nlohmann/json.hpp>

#include "requirement.hpp"

// --- Timing (functions in timing:: so ADL finds them for
// std::variant<timing::*>) ---

namespace timing {

inline void to_json(nlohmann::json& jobj, const Timing& tim) {
    std::visit(
        [&jobj](const auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, Immediately>) {
                jobj = {{"type", "Immediately"}};
            } else if constexpr (std::is_same_v<T, NextTimepoint>) {
                jobj = {{"type", "NextTimepoint"}};
            } else if constexpr (std::is_same_v<T, WithinTicks>) {
                jobj = {{"type", "WithinTicks"}, {"ticks", val.m_ticks}};
            } else if constexpr (std::is_same_v<T, ForTicks>) {
                jobj = {{"type", "ForTicks"}, {"ticks", val.m_ticks}};
            } else if constexpr (std::is_same_v<T, AfterTicks>) {
                jobj = {{"type", "AfterTicks"}, {"ticks", val.m_ticks}};
            } else if constexpr (std::is_same_v<T, Eventually>) {
                jobj = {{"type", "Eventually"}};
            } else if constexpr (std::is_same_v<T, Always>) {
                jobj = {{"type", "Always"}};
            }
        },
        tim);
}

inline void from_json(const nlohmann::json& jobj, Timing& tim) {
    const std::string type = jobj.at("type").get<std::string>();
    if (type == "Immediately") {
        tim = immediately();
    } else if (type == "NextTimepoint") {
        tim = next_timepoint();
    } else if (type == "WithinTicks") {
        tim = within_ticks(jobj.at("ticks").get<std::size_t>());
    } else if (type == "ForTicks") {
        tim = for_ticks(jobj.at("ticks").get<std::size_t>());
    } else if (type == "AfterTicks") {
        tim = after_ticks(jobj.at("ticks").get<std::size_t>());
    } else if (type == "Eventually") {
        tim = eventually();
    } else if (type == "Always") {
        tim = always();
    } else {
        throw std::invalid_argument("unknown timing type: " + type);
    }
}

}  // namespace timing

// --- Requirement ---

inline void to_json(nlohmann::json& jobj, const Requirement& req) {
    // Atom names carry the internal k_atom_prefix in memory; strip it so the
    // serialised form shows the user's original names.
    const Requirement stripped = strip_atom_prefix(req);
    jobj = {{"condition", stripped.m_condition.to_string()},
            {"condition-type",
             stripped.m_condition_type == ConditionType::Trigger ? "trigger"
                                                                 : "continual"},
            {"response", stripped.m_response.to_string()},
            {"timing", stripped.m_timing}};
    // Emitted only when locked; absence round-trips to the default
    // (weakenable).
    if (!stripped.m_weakenable) {
        jobj["weakenable"] = false;
    }
}

// --- Specification ---

inline void to_json(nlohmann::json& jobj, const Specification& spc) {
    // Requirements are stripped by to_json(Requirement) as they serialise; the
    // atom vectors have no such hook, so strip them here (once, on this node).
    const Specification stripped = strip_atom_prefix(spc);
    jobj = {{"assumptions", spc.m_assumptions},
            {"guarantees", spc.m_guarantees},
            {"in_atoms", stripped.m_in_atoms},
            {"out_atoms", stripped.m_out_atoms}};
}

namespace serialisation {

/// Constructs a Requirement from its JSON representation. Cannot be expressed
/// as a from_json ADL overload because Requirement is not
/// default-constructible.
inline Requirement requirement_from_json(const nlohmann::json& jobj) {
    const auto ctype_str = jobj.at("condition-type").get<std::string>();
    const ConditionType ctype = ctype_str == "trigger"
                                    ? ConditionType::Trigger
                                    : ConditionType::Continual;
    return Requirement(Formula(jobj.at("condition").get<std::string>()),
                       Formula(jobj.at("response").get<std::string>()),
                       jobj.at("timing").get<Timing>(), ctype,
                       jobj.value("weakenable", true));
}

/// Per-component breakdown entry stored alongside a scored specification.
struct ComponentScore {
    std::string name;
    double score = 0.0;
    double weight = 0.0;
};

/// Fitness breakdown attached to a serialised result.
struct FitnessRecord {
    double total = 0.0;
    std::vector<ComponentScore> components;
};

/// A specification paired with an optional fitness record for result storage.
struct ScoredSpecification {
    Specification spec;
    std::optional<FitnessRecord> fitness;
};

inline void to_json(nlohmann::json& jobj, const ComponentScore& cmp) {
    jobj = {{"name", cmp.name}, {"score", cmp.score}, {"weight", cmp.weight}};
}

inline void from_json(const nlohmann::json& jobj, ComponentScore& cmp) {
    cmp.name = jobj.at("name").get<std::string>();
    cmp.score = jobj.at("score").get<double>();
    cmp.weight = jobj.at("weight").get<double>();
}

inline void to_json(nlohmann::json& jobj, const FitnessRecord& frec) {
    jobj = {{"total", frec.total}, {"components", frec.components}};
}

inline void from_json(const nlohmann::json& jobj, FitnessRecord& frec) {
    frec.total = jobj.at("total").get<double>();
    frec.components = jobj.at("components").get<std::vector<ComponentScore>>();
}

inline void to_json(nlohmann::json& jobj, const ScoredSpecification& ssc) {
    ::to_json(jobj, ssc.spec);
    if (ssc.fitness.has_value()) {
        jobj["fitness"] = *ssc.fitness;
    }
}

inline void from_json(const nlohmann::json& jobj, ScoredSpecification& ssc) {
    auto parse_reqs = [](const nlohmann::json& arr) {
        std::vector<Requirement> reqs;
        reqs.reserve(arr.size());
        for (const auto& req_json : arr) {
            reqs.push_back(requirement_from_json(req_json));
        }
        return reqs;
    };
    ssc.spec = Specification(
        parse_reqs(jobj.at("assumptions")), parse_reqs(jobj.at("guarantees")),
        jobj.at("in_atoms").get<std::vector<std::string>>(),
        jobj.at("out_atoms").get<std::vector<std::string>>());
    if (jobj.contains("fitness")) {
        ssc.fitness = jobj.at("fitness").get<FitnessRecord>();
    }
}

}  // namespace serialisation

inline void from_json(const nlohmann::json& jobj, Specification& spc) {
    auto parse_reqs = [](const nlohmann::json& arr) {
        std::vector<Requirement> reqs;
        reqs.reserve(arr.size());
        for (const auto& req_json : arr) {
            reqs.push_back(serialisation::requirement_from_json(req_json));
        }
        return reqs;
    };
    spc = Specification(parse_reqs(jobj.at("assumptions")),
                        parse_reqs(jobj.at("guarantees")),
                        jobj.at("in_atoms").get<std::vector<std::string>>(),
                        jobj.at("out_atoms").get<std::vector<std::string>>());
}

namespace detail {

inline std::optional<std::string> validate_requirement_json(
    const nlohmann::json& req, const std::string& path) {
    if (!req.is_object()) {
        return path + ": expected object";
    }
    for (const char* field : {"condition", "response"}) {
        if (!req.contains(field)) {
            return path + "." + field + ": missing required field";
        }
        if (!req.at(field).is_string()) {
            return path + "." + field + ": must be a string";
        }
    }
    if (!req.contains("condition-type")) {
        return path + ".condition-type: missing required field";
    }
    if (!req.at("condition-type").is_string()) {
        return path + ".condition-type: must be a string";
    }
    {
        const std::string ctype = req.at("condition-type").get<std::string>();
        if (ctype != "trigger" && ctype != "continual") {
            return path + ".condition-type: unknown value '" + ctype + "'";
        }
    }
    if (!req.contains("timing")) {
        return path + ".timing: missing required field";
    }
    const nlohmann::json& timing = req.at("timing");
    if (!timing.is_object()) {
        return path + ".timing: must be an object";
    }
    if (!timing.contains("type") || !timing.at("type").is_string()) {
        return path + ".timing.type: must be a string";
    }
    const std::string ttype = timing.at("type").get<std::string>();
    if (ttype != "Immediately" && ttype != "NextTimepoint" &&
        ttype != "WithinTicks" && ttype != "ForTicks" &&
        ttype != "AfterTicks" && ttype != "Eventually" && ttype != "Always") {
        return path + ".timing.type: unknown value '" + ttype + "'";
    }
    if (ttype == "WithinTicks" || ttype == "ForTicks" ||
        ttype == "AfterTicks") {
        if (!timing.contains("ticks") ||
            !timing.at("ticks").is_number_unsigned()) {
            return path + ".timing.ticks: must be a non-negative integer";
        }
    }
    if (req.contains("weakenable") && !req.at("weakenable").is_boolean()) {
        return path + ".weakenable: must be a boolean";
    }
    return std::nullopt;
}

inline std::optional<std::string> validate_string_array_json(
    const nlohmann::json& jobj, const char* field) {
    if (!jobj.contains(field)) {
        return std::string(field) + ": missing required field";
    }
    if (!jobj.at(field).is_array()) {
        return std::string(field) + ": must be an array";
    }
    for (std::size_t i = 0; i < jobj.at(field).size(); ++i) {
        if (!jobj.at(field).at(i).is_string()) {
            return std::string(field) + "[" + std::to_string(i) +
                   "]: must be a string";
        }
    }
    return std::nullopt;
}

inline std::optional<std::string> validate_requirement_array_json(
    const nlohmann::json& jobj, const char* field) {
    if (!jobj.contains(field)) {
        return std::string(field) + ": missing required field";
    }
    if (!jobj.at(field).is_array()) {
        return std::string(field) + ": must be an array";
    }
    for (std::size_t i = 0; i < jobj.at(field).size(); ++i) {
        const std::string epath =
            std::string(field) + "[" + std::to_string(i) + "]";
        if (auto err = validate_requirement_json(jobj.at(field).at(i), epath)) {
            return err;
        }
    }
    return std::nullopt;
}

inline std::optional<std::string> validate_fitness_json(
    const nlohmann::json& fitness) {
    if (!fitness.is_object()) {
        return "fitness: must be an object";
    }
    if (!fitness.contains("total") || !fitness.at("total").is_number()) {
        return "fitness.total: must be a number";
    }
    if (!fitness.contains("components") ||
        !fitness.at("components").is_array()) {
        return "fitness.components: must be an array";
    }
    for (std::size_t i = 0; i < fitness.at("components").size(); ++i) {
        const nlohmann::json& comp = fitness.at("components").at(i);
        const std::string cpath =
            "fitness.components[" + std::to_string(i) + "]";
        if (!comp.is_object()) {
            return cpath + ": expected object";
        }
        if (!comp.contains("name") || !comp.at("name").is_string()) {
            return cpath + ".name: must be a string";
        }
        if (!comp.contains("score") || !comp.at("score").is_number()) {
            return cpath + ".score: must be a number";
        }
        if (!comp.contains("weight") || !comp.at("weight").is_number()) {
            return cpath + ".weight: must be a number";
        }
    }
    return std::nullopt;
}

}  // namespace detail

/// Validates that @p jobj conforms to the Specification JSON schema.
///
/// Checks required fields (assumptions, guarantees, in_atoms, out_atoms) and
/// their types, validates each requirement's condition/condition-type/response/
/// timing, and validates the optional fitness block if present.
///
/// @return A human-readable error description, or std::nullopt if the object
///         is valid.
inline std::optional<std::string> validate_specification_json(
    const nlohmann::json& jobj) {
    if (!jobj.is_object()) {
        return std::string("root: expected object");
    }
    for (const char* field : {"in_atoms", "out_atoms"}) {
        if (auto err = detail::validate_string_array_json(jobj, field)) {
            return err;
        }
    }
    for (const char* field : {"assumptions", "guarantees"}) {
        if (auto err = detail::validate_requirement_array_json(jobj, field)) {
            return err;
        }
    }
    if (jobj.contains("fitness")) {
        return detail::validate_fitness_json(jobj.at("fitness"));
    }
    return std::nullopt;
}

/// Reads a JSON file at @p path and deserialises it as a ScoredSpecification.
/// Throws std::runtime_error on I/O failure, malformed JSON, or schema
/// violations.
inline serialisation::ScoredSpecification load_scored_specification(
    const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open input file: " + path);
    }
    nlohmann::json json_in;
    try {
        file >> json_in;
    } catch (const nlohmann::json::parse_error& exc) {
        throw std::runtime_error("JSON parse error in " + path + ": " +
                                 exc.what());
    }
    if (const auto err = validate_specification_json(json_in)) {
        throw std::runtime_error("invalid specification in " + path + ": " +
                                 *err);
    }
    try {
        auto scored = json_in.get<serialisation::ScoredSpecification>();
        scored.spec = add_atom_prefix(scored.spec);
        return scored;
    } catch (const nlohmann::json::exception& exc) {
        throw std::runtime_error("invalid specification in " + path + ": " +
                                 exc.what());
    }
}

/// Reads a JSON file at @p path and deserialises it as a Specification.
/// Throws std::runtime_error on I/O failure, malformed JSON, or schema
/// violations.
inline Specification load_specification(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open input file: " + path);
    }
    nlohmann::json json_in;
    try {
        file >> json_in;
    } catch (const nlohmann::json::parse_error& exc) {
        throw std::runtime_error("JSON parse error in " + path + ": " +
                                 exc.what());
    }
    if (const auto err = validate_specification_json(json_in)) {
        throw std::runtime_error("invalid specification in " + path + ": " +
                                 *err);
    }
    try {
        return add_atom_prefix(json_in.get<Specification>());
    } catch (const nlohmann::json::exception& exc) {
        throw std::runtime_error("invalid specification in " + path + ": " +
                                 exc.what());
    }
}
