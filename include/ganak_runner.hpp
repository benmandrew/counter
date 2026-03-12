#pragma once

#include <string>

#include "transfer_matrix.hpp"

std::string ganak_executable_path();

Count run_ganak_on_dimacs(const std::string& dimacs_path, unsigned seed = 1);
