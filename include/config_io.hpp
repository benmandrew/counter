#pragma once

/// @file config_io.hpp
/// @brief Parses a TOML configuration file into a Config struct.

#include <filesystem>
#include <string>

#include "config.hpp"

/// Parses @p path as a TOML configuration file and returns a Config with
/// overridden values. Fields absent from the file take their defaults from
/// Config{}.
///
/// @param path  Path to the TOML file (must exist and be readable)
/// @return      A Config with values from the file merged over the defaults
/// @throws std::runtime_error if the file does not exist, cannot be read,
///         contains invalid TOML, uses an unknown key, or has a value outside
///         the valid range for its field
Config config_from_toml(const std::filesystem::path& path);

/// Parses @p content as TOML and returns a Config with overridden values.
/// Fields absent from @p content take their defaults from Config{}.
///
/// @param content  TOML-formatted string
/// @return         A Config with values from content merged over the defaults
/// @throws std::runtime_error if content is invalid TOML, uses an unknown key,
///         or has a value outside the valid range for its field
Config config_from_toml_string(const std::string& content);
