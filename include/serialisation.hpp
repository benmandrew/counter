#pragma once

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
    } else {
        throw std::invalid_argument("unknown timing type: " + type);
    }
}

}  // namespace timing

// --- Requirement ---

inline void to_json(nlohmann::json& jobj, const Requirement& req) {
    jobj = {{"trigger", req.m_trigger.to_string()},
            {"response", req.m_response.to_string()},
            {"timing", req.m_timing}};
}

// --- Specification ---

inline void to_json(nlohmann::json& jobj, const Specification& spc) {
    jobj = {{"assumptions", spc.m_assumptions},
            {"guarantees", spc.m_guarantees},
            {"in_atoms", spc.m_in_atoms},
            {"out_atoms", spc.m_out_atoms}};
}

namespace serialisation {

/// Constructs a Requirement from its JSON representation. Cannot be expressed
/// as a from_json ADL overload because Requirement is not
/// default-constructible.
inline Requirement requirement_from_json(const nlohmann::json& jobj) {
    return Requirement(Formula(jobj.at("trigger").get<std::string>()),
                       Formula(jobj.at("response").get<std::string>()),
                       jobj.at("timing").get<Timing>());
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

/// Reads a JSON file at @p path and deserialises it as a Specification.
/// Throws std::runtime_error on I/O failure, malformed JSON, or missing fields.
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
    try {
        return json_in.get<Specification>();
    } catch (const nlohmann::json::exception& exc) {
        throw std::runtime_error("invalid specification in " + path + ": " +
                                 exc.what());
    }
}
