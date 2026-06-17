#include "filter/bloat.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace {

std::size_t max_formula_size(const Specification& spec) {
    std::size_t max = 0;
    for (const Requirement& req : spec.m_assumptions) {
        max = std::max(max, req.m_trigger.n_subformulae());
        max = std::max(max, req.m_response.n_subformulae());
    }
    for (const Requirement& req : spec.m_guarantees) {
        max = std::max(max, req.m_trigger.n_subformulae());
        max = std::max(max, req.m_response.n_subformulae());
    }
    return max;
}

bool any_formula_exceeds(const Specification& spec, std::size_t cap) {
    auto req_exceeds = [cap](const Requirement& req) {
        return req.m_trigger.n_subformulae() > cap ||
               req.m_response.n_subformulae() > cap;
    };
    return std::any_of(spec.m_assumptions.begin(), spec.m_assumptions.end(),
                       req_exceeds) ||
           std::any_of(spec.m_guarantees.begin(), spec.m_guarantees.end(),
                       req_exceeds);
}

}  // namespace

FilterFunction make_bloat_cap_filter(const Specification& original,
                                     double max_ratio) {
    const std::size_t original_max = max_formula_size(original);
    return {"bloat-cap",
            [original_max, max_ratio](const std::vector<Specification>& pop) {
                if (original_max == 0) {
                    return pop;
                }
                const auto cap = static_cast<std::size_t>(
                    max_ratio * static_cast<double>(original_max));
                std::vector<Specification> survivors;
                survivors.reserve(pop.size());
                for (const Specification& spec : pop) {
                    if (!any_formula_exceeds(spec, cap)) {
                        survivors.push_back(spec);
                    }
                }
                return survivors;
            }};
}
