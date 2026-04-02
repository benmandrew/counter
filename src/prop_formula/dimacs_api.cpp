#include <sstream>
#include <string>
#include <vector>

#include "internal.hpp"

std::string Formula::to_dimacs() const {
    const prop_formula_internal::DimacsCnf cnf =
        prop_formula_internal::encode_dimacs(m_impl->m_nodes);

    std::ostringstream stream;
    stream << "p cnf " << cnf.m_variable_count << ' ' << cnf.m_clauses.size()
           << "\n";
    for (const std::vector<int>& clause : cnf.m_clauses) {
        for (const int literal : clause) {
            stream << literal << ' ';
        }
        stream << "0\n";
    }
    return stream.str();
}
