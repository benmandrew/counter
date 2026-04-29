#pragma once

#include <string>

std::string black_executable_path();

bool check_satisfiability(const std::string& ltl_formula);
