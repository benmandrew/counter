#include "fitness/syntactic_similarity.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <type_traits>
#include <variant>
#include <vector>

#include "prop_formula.hpp"
#include "requirement.hpp"

namespace {

// Geometric ratio r and small fixed weight w for the timing measure.
// μ(ForTicks{N}) = μ(WithinTicks{N}) = μ(AfterTicks{N}) = r^N
// μ(Immediately) = μ(NextTimepoint) = μ(Eventually) = w
constexpr double kTimingGeoRatio = 0.5;
constexpr double kTimingDiscreteWeight = 0.01;

// Σ_{k=1}^{n} r^k
double geo_sum_up_to(int n_terms) {
    if (n_terms <= 0) {
        return 0.0;
    }
    return kTimingGeoRatio * (1.0 - std::pow(kTimingGeoRatio, n_terms)) /
           (1.0 - kTimingGeoRatio);
}

// Σ_{k=min_exp}^{∞} r^k = r^min_exp / (1 - r)
double geo_sum_from(int min_exp) {
    return std::pow(kTimingGeoRatio, min_exp) / (1.0 - kTimingGeoRatio);
}

// Largest k such that ForTicks{k} ∈ ↓tim. Returns 0 if none.
int max_for_index(const Timing& tim) {
    if (const auto* for_ptr = std::get_if<timing::ForTicks>(&tim)) {
        return static_cast<int>(for_ptr->m_ticks);
    }
    return 0;
}

// Smallest k such that WithinTicks{k} ∈ ↓tim.
// Returns 0 (sentinel for "none") when ↓tim contains no WithinTicks elements.
int min_within_index(const Timing& tim) {
    return std::visit(
        [](const auto& val) -> int {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, timing::ForTicks> ||
                          std::is_same_v<T, timing::Immediately> ||
                          std::is_same_v<T, timing::NextTimepoint>) {
                return 1;
            } else if constexpr (std::is_same_v<T, timing::WithinTicks>) {
                return static_cast<int>(val.m_ticks);
            } else if constexpr (std::is_same_v<T, timing::AfterTicks>) {
                return static_cast<int>(val.m_ticks) + 1;
            } else {
                return 0;  // Eventually: no WithinTicks in ↓tim
            }
        },
        tim);
}

bool has_immediately(const Timing& tim) {
    return std::holds_alternative<timing::ForTicks>(tim) ||
           std::holds_alternative<timing::Immediately>(tim);
}

bool has_next_timepoint(const Timing& tim) {
    return std::holds_alternative<timing::ForTicks>(tim) ||
           std::holds_alternative<timing::NextTimepoint>(tim);
}

// Returns N if tim == AfterTicks{N}, otherwise -1.
int after_ticks_self(const Timing& tim) {
    if (const auto* aft = std::get_if<timing::AfterTicks>(&tim)) {
        return static_cast<int>(aft->m_ticks);
    }
    return -1;
}

// μ(↓tim) — measure of the downward closure of tim in the timing partial order.
double mu_downset(const Timing& tim) {
    double result = kTimingDiscreteWeight;  // Eventually ∈ ↓tim always
    result += geo_sum_up_to(max_for_index(tim));
    if (has_immediately(tim)) {
        result += kTimingDiscreteWeight;
    }
    if (has_next_timepoint(tim)) {
        result += kTimingDiscreteWeight;
    }
    const int min_w = min_within_index(tim);
    if (min_w > 0) {
        result += geo_sum_from(min_w);
    }
    const int aft = after_ticks_self(tim);
    if (aft >= 0) {
        result += std::pow(kTimingGeoRatio, aft);
    }
    return result;
}

// μ(↓tim1 ∩ ↓tim2) — element e is in intersection iff e ≤ tim1 and e ≤ tim2.
double mu_intersection(const Timing& tim1, const Timing& tim2) {
    double result = kTimingDiscreteWeight;  // Eventually always in both
    result += geo_sum_up_to(std::min(max_for_index(tim1), max_for_index(tim2)));
    if (has_immediately(tim1) && has_immediately(tim2)) {
        result += kTimingDiscreteWeight;
    }
    if (has_next_timepoint(tim1) && has_next_timepoint(tim2)) {
        result += kTimingDiscreteWeight;
    }
    const int min_w1 = min_within_index(tim1);
    const int min_w2 = min_within_index(tim2);
    if (min_w1 > 0 && min_w2 > 0) {
        result += geo_sum_from(std::max(min_w1, min_w2));
    }
    // AfterTicks{k} is only in ↓AfterTicks{k}, so it appears in the
    // intersection only when both timings are the same AfterTicks{k}.
    const int aft1 = after_ticks_self(tim1);
    const int aft2 = after_ticks_self(tim2);
    if (aft1 >= 0 && aft1 == aft2) {
        result += std::pow(kTimingGeoRatio, aft1);
    }
    return result;
}

// Jaccard similarity on downward closures:
//   synSim_time(tim, tim') = μ(↓tim ∩ ↓tim') / μ(↓tim ∪ ↓tim')
double timing_syntactic_similarity(const Timing& tim, const Timing& tim_other) {
    const double mu_tim = mu_downset(tim);
    const double mu_other = mu_downset(tim_other);
    const double mu_inter = mu_intersection(tim, tim_other);
    const double mu_union = mu_tim + mu_other - mu_inter;
    return mu_inter / mu_union;
}

Formula conjoin_field(const Specification& spec, Formula Requirement::* field) {
    std::optional<Formula> conj;
    auto accumulate = [&](const std::vector<Requirement>& reqs) {
        for (const Requirement& req : reqs) {
            if (!conj) {
                conj = req.*field;
            } else {
                conj =
                    Formula::make_binary(Formula::Kind::And, *conj, req.*field);
            }
        }
    };
    accumulate(spec.m_assumptions);
    accumulate(spec.m_guarantees);
    assert(conj.has_value());
    return *conj;
}

Formula conjoin_triggers(const Specification& spec) {
    return conjoin_field(spec, &Requirement::m_condition);
}

Formula conjoin_responses(const Specification& spec) {
    return conjoin_field(spec, &Requirement::m_response);
}

double average_timing_similarity(const Specification& spec1,
                                 const Specification& spec2) {
    assert(spec1.m_assumptions.size() == spec2.m_assumptions.size());
    assert(spec1.m_guarantees.size() == spec2.m_guarantees.size());
    const std::size_t total =
        spec1.m_assumptions.size() + spec1.m_guarantees.size();
    assert(total > 0);
    double sum = 0.0;
    for (std::size_t i = 0; i < spec1.m_assumptions.size(); ++i) {
        sum += timing_syntactic_similarity(spec1.m_assumptions[i].m_timing,
                                           spec2.m_assumptions[i].m_timing);
    }
    for (std::size_t i = 0; i < spec1.m_guarantees.size(); ++i) {
        sum += timing_syntactic_similarity(spec1.m_guarantees[i].m_timing,
                                           spec2.m_guarantees[i].m_timing);
    }
    return sum / static_cast<double>(total);
}

}  // namespace

double syntactic_similarity(const Requirement& requirement,
                            const Requirement& other_requirement,
                            const Config& cfg) {
    double condition_similarity = requirement.m_condition.syntactic_similarity(
        other_requirement.m_condition);
    double response_similarity = requirement.m_response.syntactic_similarity(
        other_requirement.m_response);
    double timing_similarity = timing_syntactic_similarity(
        requirement.m_timing, other_requirement.m_timing);
    const double total_weight = cfg.syntactic_weight_trigger +
                                cfg.syntactic_weight_response +
                                cfg.syntactic_weight_timing;
    return (cfg.syntactic_weight_trigger * condition_similarity +
            cfg.syntactic_weight_response * response_similarity +
            cfg.syntactic_weight_timing * timing_similarity) /
           total_weight;
}

double syntactic_similarity(const Specification& specification,
                            const Specification& other_specification,
                            const Config& cfg) {
    assert((!specification.m_assumptions.empty() ||
            !specification.m_guarantees.empty()) &&
           (!other_specification.m_assumptions.empty() ||
            !other_specification.m_guarantees.empty()));
    const double trigger_similarity =
        conjoin_triggers(specification)
            .syntactic_similarity(conjoin_triggers(other_specification));
    const double response_similarity =
        conjoin_responses(specification)
            .syntactic_similarity(conjoin_responses(other_specification));
    double timing_similarity =
        average_timing_similarity(specification, other_specification);
    const double total_weight = cfg.syntactic_weight_trigger +
                                cfg.syntactic_weight_response +
                                cfg.syntactic_weight_timing;
    return (cfg.syntactic_weight_trigger * trigger_similarity +
            cfg.syntactic_weight_response * response_similarity +
            cfg.syntactic_weight_timing * timing_similarity) /
           total_weight;
}
