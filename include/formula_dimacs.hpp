#pragma once

#include <string>
#include <vector>

struct DimacsCnf {
    int variable_count;
    std::vector<std::vector<int>> clauses;

    std::string to_dimacs() const;
};

DimacsCnf formula_to_dimacs(const std::string& formula);
