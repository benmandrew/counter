#pragma once

#include <cstddef>

#include "transfer_matrix.hpp"

Count count_traces(const TransferSystem& system, std::size_t step_count);
