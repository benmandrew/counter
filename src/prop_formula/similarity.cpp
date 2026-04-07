#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "internal.hpp"

namespace {

struct SubformulaKey {
    prop_formula_internal::NodeType m_type;
    std::size_t m_left = 0;
    std::size_t m_right = 0;
    std::string_view m_variable;
};

constexpr std::size_t hash_combine(std::size_t seed, std::size_t value) {
    // Mixes bits from seed and value to reduce clustering of similar keys.
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));
}

struct SubformulaKeyHash {
    std::size_t operator()(const SubformulaKey& key) const {
        std::size_t seed = std::hash<int>{}(static_cast<int>(key.m_type));
        seed = hash_combine(seed, std::hash<std::size_t>{}(key.m_left));
        seed = hash_combine(seed, std::hash<std::size_t>{}(key.m_right));
        seed =
            hash_combine(seed, std::hash<std::string_view>{}(key.m_variable));
        return seed;
    }
};

struct SubformulaKeyEqual {
    bool operator()(const SubformulaKey& lhs, const SubformulaKey& rhs) const {
        return lhs.m_type == rhs.m_type && lhs.m_left == rhs.m_left &&
               lhs.m_right == rhs.m_right && lhs.m_variable == rhs.m_variable;
    }
};

std::unordered_map<std::size_t, std::size_t> collect_subformula_ids(
    const std::vector<prop_formula_internal::Node>& input_nodes,
    std::unordered_map<SubformulaKey, std::size_t, SubformulaKeyHash,
                       SubformulaKeyEqual>& id_pool,
    std::size_t& next_id) {
    std::vector<std::size_t> ids(input_nodes.size(), 0);
    std::unordered_map<std::size_t, std::size_t> counts;
    counts.reserve(input_nodes.size());
    for (std::size_t index = 0; index < input_nodes.size(); ++index) {
        const prop_formula_internal::Node& node = input_nodes[index];
        SubformulaKey key{};
        key.m_type = node.m_type;
        switch (node.m_type) {
            case prop_formula_internal::NodeType::Variable:
                key.m_variable = node.m_variable;
                break;
            case prop_formula_internal::NodeType::Not:
                key.m_left = ids[node.m_left];
                break;
            case prop_formula_internal::NodeType::And:
                key.m_left = ids[node.m_left];
                key.m_right = ids[node.m_right];
                break;
            case prop_formula_internal::NodeType::Or:
                key.m_left = ids[node.m_left];
                key.m_right = ids[node.m_right];
                break;
            case prop_formula_internal::NodeType::Implies:
                key.m_left = ids[node.m_left];
                key.m_right = ids[node.m_right];
                break;
            case prop_formula_internal::NodeType::Iff:
                key.m_left = ids[node.m_left];
                key.m_right = ids[node.m_right];
                break;
        }
        const auto [it, inserted] = id_pool.emplace(key, next_id);
        if (inserted) {
            ids[index] = next_id;
            ++next_id;
        } else {
            ids[index] = it->second;
        }
        ++counts[ids[index]];
    }
    return counts;
}

std::size_t count_shared_subformulae(
    const std::vector<prop_formula_internal::Node>& nodes,
    const std::vector<prop_formula_internal::Node>& other_nodes) {
    std::unordered_map<SubformulaKey, std::size_t, SubformulaKeyHash,
                       SubformulaKeyEqual>
        id_pool;
    std::size_t next_id = 1;
    id_pool.reserve((nodes.size() + other_nodes.size()) * 2);
    const auto counts = collect_subformula_ids(nodes, id_pool, next_id);
    const auto other_counts =
        collect_subformula_ids(other_nodes, id_pool, next_id);
    std::size_t shared_subformulae = 0;
    for (const auto& [signature, count] : counts) {
        const auto other_it = other_counts.find(signature);
        if (other_it == other_counts.end()) {
            continue;
        }
        shared_subformulae += std::min(count, other_it->second);
    }
    return shared_subformulae;
}

}  // namespace

double Formula::syntactic_similarity(const Formula& other) const {
    if (this->n_subformulae() == 0 || other.n_subformulae() == 0) {
        return 1.0;
    }
    const double shared_subformulae =
        static_cast<double>(this->shared_subformulae(other));
    return 0.5 *
           (shared_subformulae / static_cast<double>(this->n_subformulae()) +
            shared_subformulae / static_cast<double>(other.n_subformulae()));
}

size_t Formula::n_subformulae() const { return m_impl->m_nodes.size(); }

size_t Formula::shared_subformulae(const Formula& other) const {
    return count_shared_subformulae(m_impl->m_nodes, other.m_impl->m_nodes);
}
