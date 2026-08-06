#include "cli.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

void print_help(const char* prog) {
    std::cout
        << "Usage: " << prog
        << " --input <spec.json> --output-dir <dir> [--seed <n>]\n"
        << "\n"
        << "Repair an unrealisable FRETISH specification using a genetic\n"
        << "algorithm. The input specification is read from a JSON file and\n"
        << "the algorithm evolves a population of candidate repairs, scoring\n"
        << "each by syntactic similarity, semantic similarity, Halstead\n"
        << "complexity, and LTL realisability.\n"
        << "\n"
        << "Options:\n"
        << "  --input <spec.json>  Path to the input specification "
           "(required).\n"
        << "                       Accepts plain Specification JSON or a\n"
        << "                       ScoredSpecification with an optional\n"
        << "                       \"fitness\" field.\n"
        << "  --output-dir <dir>   Directory to write maximal realizable\n"
        << "                       repairs to as repair_0.json, "
           "repair_1.json,\n"
        << "                       ... (required; directory must already "
           "exist).\n"
        << "  --config <file>      Path to a TOML configuration file.\n"
        << "                       Absent keys use built-in defaults.\n"
        << "  --format <fmt>       Input format: fretish or tlsf. If omitted,\n"
        << "                       inferred from the --input extension (a\n"
        << "                       .tlsf file is auto-detected as TLSF, any\n"
        << "                       other extension as FRETISH).\n"
        << "  --seed <n>           RNG seed for reproducible runs. If omitted\n"
        << "                       a random seed is chosen and printed.\n"
        << "  --dashboard          Stream progress to\n"
        << "                       <output-dir>/progress.jsonl and write a\n"
        << "                       live dashboard page beside it. Equivalent\n"
        << "                       to [runtime] dashboard = true.\n"
        << "  --cpu-report         Print a CPU-attribution report at exit:\n"
        << "                       this process's own code against the\n"
        << "                       external CLI tools, and per tool.\n"
        << "  --version            Print the git commit this binary was "
           "built\n"
        << "                       from as commit=, commit_short= and dirty=\n"
        << "                       lines, and exit.\n"
        << "  -h, --help           Show this help message and exit.\n"
        << "\n"
        << "Input format (examples/takeoff/spec.json):\n"
        << "  {\n"
        << "    \"assumptions\": [],\n"
        << "    \"guarantees\":  [ { \"trigger\": \"<formula>\",\n"
        << "                        \"response\": \"<formula>\",\n"
        << "                        \"timing\":   { \"type\": \"Immediately\" "
           "} } ],\n"
        << "    \"in_atoms\":  [\"a\", \"b\"],\n"
        << "    \"out_atoms\": [\"x\", \"y\"]\n"
        << "  }\n"
        << "\n"
        << "Timing types: Immediately, NextTimepoint, Eventually, Always,\n"
        << "              WithinTicks {\"ticks\": n}, ForTicks {\"ticks\": "
           "n},\n"
        << "              AfterTicks  {\"ticks\": n}\n";
}

std::optional<bool> resolve_is_tlsf(
    const std::optional<std::string>& format_arg,
    const std::string& input_path) {
    if (!format_arg.has_value()) {
        return std::filesystem::path(input_path).extension() == ".tlsf";
    }
    if (*format_arg == "tlsf") {
        return true;
    }
    if (*format_arg == "fretish") {
        return false;
    }
    return std::nullopt;
}
