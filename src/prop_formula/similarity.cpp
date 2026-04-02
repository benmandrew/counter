#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "internal.hpp"

namespace {

std::size_t count_shared_subformulae(
    const std::vector<prop_formula_internal::Node>& nodes,
    const std::vector<prop_formula_internal::Node>& other_nodes) {
    auto collect_subformula_signatures =
        [](const std::vector<prop_formula_internal::Node>& input_nodes) {
            std::vector<std::string> signatures(input_nodes.size());
            std::unordered_map<std::string, std::size_t> counts;
            for (std::size_t index = 0; index < input_nodes.size(); ++index) {
                const prop_formula_internal::Node& node = input_nodes[index];
                switch (node.m_type) {
                    case prop_formula_internal::NodeType::Variable:
                        signatures[index] = "V(" + node.m_variable + ")";
                        break;
                    case prop_formula_internal::NodeType::Not:
                        signatures[index] =
                            "N(" + signatures[node.m_left] + ")";
                        break;
                    case prop_formula_internal::NodeType::And:
                        signatures[index] = "A(" + signatures[node.m_left] +
                                            "," + signatures[node.m_right] +
                                            ")";
                        break;
                    case prop_formula_internal::NodeType::Or:
                        signatures[index] = "O(" + signatures[node.m_left] +
                                            "," + signatures[node.m_right] +
                                            ")";
                        break;
                    case prop_formula_internal::NodeType::Implies:
                        signatures[index] = "I(" + signatures[node.m_left] +
                                            "," + signatures[node.m_right] +
                                            ")";
                        break;
                    case prop_formula_internal::NodeType::Iff:
                        signatures[index] = "E(" + signatures[node.m_left] +
                                            "," + signatures[node.m_right] +
                                            ")";
                        break;
                }
                ++counts[signatures[index]];
            }

            return counts;
        };

    const auto counts = collect_subformula_signatures(nodes);
    const auto other_counts = collect_subformula_signatures(other_nodes);
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
