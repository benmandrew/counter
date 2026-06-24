#include "fitness/halstead.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>
#include <string>
#include <type_traits>
#include <variant>

namespace {

struct RawCounts {
    std::set<std::string> operators;
    std::set<std::string> operands;
    std::size_t n1 = 0;
    std::size_t n2 = 0;

    void merge(const RawCounts& other) {
        operators.insert(other.operators.begin(), other.operators.end());
        operands.insert(other.operands.begin(), other.operands.end());
        n1 += other.n1;
        n2 += other.n2;
    }

    [[nodiscard]] HalsteadCounts to_counts() const {
        return {operators.size(), operands.size(), n1, n2};
    }
};

const char* connective_string(Formula::Kind kind) {
    switch (kind) {
        case Formula::Kind::Not:
            return "!";
        case Formula::Kind::And:
            return "&";
        case Formula::Kind::Or:
            return "|";
        case Formula::Kind::Implies:
            return "->";
        case Formula::Kind::Iff:
            return "<->";
        case Formula::Kind::Atom:
        default:
            assert(false);
            __builtin_unreachable();
    }
}

void traverse_formula(const Formula& formula, RawCounts& counts) {
    switch (formula.kind()) {
        case Formula::Kind::Atom: {
            const auto name = formula.atom_name();
            if (!name.has_value()) {
                assert(false);
                __builtin_unreachable();
            }
            counts.operands.insert(*name);
            ++counts.n2;
            break;
        }
        case Formula::Kind::Not: {
            counts.operators.insert(connective_string(formula.kind()));
            ++counts.n1;
            const auto child = formula.unary_child();
            if (!child.has_value()) {
                assert(false);
                __builtin_unreachable();
            }
            traverse_formula(*child, counts);
            break;
        }
        default: {
            counts.operators.insert(connective_string(formula.kind()));
            ++counts.n1;
            const auto children = formula.binary_children();
            if (!children.has_value()) {
                assert(false);
                __builtin_unreachable();
            }
            traverse_formula(children->first, counts);
            traverse_formula(children->second, counts);
            break;
        }
    }
}

RawCounts count_formula(const Formula& formula) {
    RawCounts counts;
    traverse_formula(formula, counts);
    return counts;
}

RawCounts count_timing(const Timing& timing) {
    RawCounts counts;
    std::visit(
        [&counts](const auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, timing::Immediately>) {
                counts.operators.insert("imm");
                ++counts.n1;
            } else if constexpr (std::is_same_v<T, timing::NextTimepoint>) {
                counts.operators.insert("next");
                ++counts.n1;
            } else if constexpr (std::is_same_v<T, timing::Eventually>) {
                counts.operators.insert("eventually");
                ++counts.n1;
            } else {
                if constexpr (std::is_same_v<T, timing::WithinTicks>) {
                    counts.operators.insert("within");
                } else if constexpr (std::is_same_v<T, timing::ForTicks>) {
                    counts.operators.insert("for");
                } else {
                    counts.operators.insert("after");
                }
                ++counts.n1;
                counts.operands.insert("ticks:" + std::to_string(val.m_ticks));
                ++counts.n2;
            }
        },
        timing);
    return counts;
}

RawCounts count_requirement(const Requirement& req) {
    RawCounts counts = count_formula(req.m_condition);
    counts.merge(count_formula(req.m_response));
    counts.merge(count_timing(req.m_timing));
    return counts;
}

RawCounts count_specification(const Specification& spec) {
    RawCounts counts;
    for (const Requirement& req : spec.m_assumptions) {
        counts.merge(count_requirement(req));
    }
    for (const Requirement& req : spec.m_guarantees) {
        counts.merge(count_requirement(req));
    }
    return counts;
}

}  // namespace

HalsteadCounts halstead_counts(const Formula& formula) {
    return count_formula(formula).to_counts();
}

HalsteadCounts halstead_counts(const Requirement& requirement) {
    return count_requirement(requirement).to_counts();
}

HalsteadCounts halstead_counts(const Specification& specification) {
    return count_specification(specification).to_counts();
}

double halstead_volume(const HalsteadCounts& counts) {
    const std::size_t eta = counts.eta1 + counts.eta2;
    if (eta <= 1) {
        return 0.0;
    }
    const std::size_t length = counts.n1 + counts.n2;
    return static_cast<double>(length) * std::log2(static_cast<double>(eta));
}

double halstead_fitness(const Specification& specification,
                        const Specification& original) {
    const double volume = halstead_volume(halstead_counts(specification));
    const double original_volume = halstead_volume(halstead_counts(original));
    if (volume <= 0.0) {
        return 1.0;
    }
    if (original_volume <= 0.0) {
        return 0.0;
    }
    return std::min(1.0, original_volume / volume);
}
