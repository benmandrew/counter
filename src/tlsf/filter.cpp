#include "tlsf/filter.hpp"

#include <unordered_set>
#include <vector>

#include "runner/black.hpp"
#include "tlsf/specification.hpp"

FilterFunctionT<tlsf::Specification> tlsf_make_dedup_filter() {
    return {"dedup", [](const std::vector<tlsf::Specification>& pop) {
                std::unordered_set<tlsf::Specification> seen;
                seen.reserve(pop.size());
                std::vector<tlsf::Specification> survivors;
                survivors.reserve(pop.size());
                for (const tlsf::Specification& spec : pop) {
                    if (seen.insert(spec).second) {
                        survivors.push_back(spec);
                    }
                }
                return survivors;
            }};
}

FilterFunctionT<tlsf::Specification> tlsf_make_assumption_sat_filter() {
    return {"assumption-sat", [](const std::vector<tlsf::Specification>& pop) {
                std::vector<tlsf::Specification> survivors;
                survivors.reserve(pop.size());
                for (const tlsf::Specification& spec : pop) {
                    const bool no_assumptions = spec.m_initially.empty() &&
                                                spec.m_require.empty() &&
                                                spec.m_assume.empty();
                    if (no_assumptions ||
                        global_sat_checker()
                            .check_satisfiability(spec.assumption_ltl())
                            .value_or(true)) {
                        survivors.push_back(spec);
                    }
                }
                return survivors;
            }};
}
