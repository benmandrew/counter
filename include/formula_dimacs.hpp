#pragma once

#include <string>
#include <vector>

struct DimacsCnf {
    int m_variable_count;
    std::vector<std::vector<int>> m_clauses;

    std::string to_dimacs() const;
};

DimacsCnf formula_to_dimacs(const std::string& formula);
