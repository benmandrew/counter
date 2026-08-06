#pragma once

#include <optional>
#include <string>
#include <vector>

// Argument handling shared by the CLI drivers (counter, compare, realize, ltl
// and mucs). Each driver still owns its own usage text and its own flags; only
// the pieces every one of them needs identically live here.

bool has_flag(int argc, const char* const* argv, const char* flag);

std::optional<std::string> parse_string_arg(int argc, const char* const* argv,
                                            const char* flag);

// Answers --version and -h/--help, reporting usage through @p print_usage so
// each driver keeps its own text and output stream. True when one of them
// fired and the caller should exit successfully.
//
// Both flags report on the binary rather than on a run, so they are answered
// before anything else: interrogating a binary must not require valid
// arguments, and --version in particular is what a harness calls to find out
// what it is about to run.
bool handle_info_flags(int argc, const char* const* argv,
                       void (*print_usage)(const char*));

std::vector<std::string> collect_argument_paths(int argc,
                                                const char* const* argv);

// Nullopt when the file cannot be opened. Each driver words that failure its
// own way -- one throws, the others print and return -- so it is reported here
// as absence rather than decided.
std::optional<std::string> read_file_contents(const std::string& path);
