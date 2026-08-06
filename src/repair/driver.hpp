#pragma once

#include <optional>
#include <string>

#include "config.hpp"

// The two top-level repair runs counter dispatches to, each returning the
// process exit status.
//
// @p seed_arg is the unparsed --seed value rather than a number: it is read
// where the run starts, so validating the rest of the command line never
// depends on it.

int run_tlsf_repair(const Config& cfg, const std::string& input_path,
                    const std::string& output_dir,
                    const std::optional<std::string>& seed_arg);

int run_fretish_repair(const Config& cfg, const std::string& input_path,
                       const std::string& output_dir,
                       const std::optional<std::string>& seed_arg);
