#pragma once

#include <cstddef>

#include "transfer_matrix.hpp"

CountMatrix matrix_power(const CountMatrix& matrix, std::size_t exponent);

Count count_traces(const TransferSystem& system, std::size_t trace_length);
