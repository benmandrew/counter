#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "config.hpp"
#include "driver_support.hpp"
#include "runner/black.hpp"
#include "thread_pool.hpp"
#include "tlsf/filter.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/specification.hpp"

// Runs counter's TLSF maximality filter over a directory of .tlsf files that
// counter did not produce, so a foreign tool's output can be measured for
// semantic diversity on the same definition counter applies to its own.
//
// Two numbers, because they answer different questions and AuRUS's own
// MaximalSolutions filter conflates them. "maximal" is the filter counter runs:
// keep every spec no other spec strictly dominates, so a whole equivalence
// class survives together. "classes" quotients those survivors by mutual
// implication, which is the count of genuinely distinct strongest repairs. A
// filter that keeps one arbitrary member per class reports the second number
// while looking like the first.

namespace {

struct Args {
    std::vector<std::string> paths;
    std::size_t jobs{0};
    std::int64_t timeout_s{20};
};

void print_usage(const char* prog) {
    std::cerr
        << "Usage: " << prog << " <dir-or-file>... [--jobs N] [--timeout S]\n"
        << "\n"
        << "Reports the maximal subset of a set of basic-TLSF specifications\n"
        << "under the implication order (A dominates B when A implies B and B\n"
        << "does not imply A), then quotients the survivors by mutual\n"
        << "implication. Directory arguments contribute every .tlsf file in\n"
        << "them, non-recursively.\n"
        << "\n"
        << "  --jobs N     Solver calls in flight (default: hardware "
           "concurrency).\n"
        << "  --timeout S  Per-black-call budget in seconds (default: 20).\n"
        << "  --version    Print the git commit this binary was built from.\n";
}

// Hand-rolled rather than through find_unknown_arg and collect_argument_paths,
// which between them assume a driver whose paths are all flag values: the first
// reports every positional as unknown and the second collects the flags as
// paths. This one takes a variable number of positional arguments, so it walks
// argv once and still refuses an unrecognised flag rather than ignoring it.
std::optional<Args> parse_args(int argc, const char* const* argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == nullptr) {
            continue;
        }
        const std::string arg(argv[i]);
        const bool is_jobs = arg == "--jobs";
        if (is_jobs || arg == "--timeout") {
            if (i + 1 >= argc || argv[i + 1] == nullptr) {
                std::cerr << arg << " expects a value\n";
                return std::nullopt;
            }
            const std::optional<std::size_t> value = parse_seed(argv[++i]);
            if (!value.has_value() || *value == 0) {
                std::cerr << arg << " expects a positive integer\n";
                return std::nullopt;
            }
            if (is_jobs) {
                args.jobs = *value;
            } else {
                args.timeout_s = static_cast<std::int64_t>(*value);
            }
            continue;
        }
        if (arg.rfind("--", 0) == 0) {
            std::cerr << "unknown argument: " << arg << "\n";
            return std::nullopt;
        }
        args.paths.push_back(arg);
    }
    if (args.paths.empty()) {
        return std::nullopt;
    }
    return args;
}

std::vector<std::string> expand_paths(const std::vector<std::string>& paths) {
    std::vector<std::string> files;
    for (const std::string& path : paths) {
        if (std::filesystem::is_directory(path)) {
            for (const auto& entry :
                 std::filesystem::directory_iterator(path)) {
                if (entry.path().extension() == ".tlsf") {
                    files.push_back(entry.path().string());
                }
            }
        } else {
            files.push_back(path);
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

// The survivors' partition into mutual-implication classes, and how many there
// are. Separate from `main` because the pairwise sweep is the one part of this
// tool that is an algorithm rather than plumbing.
struct Quotient {
    std::vector<std::size_t> m_class_of;
    std::size_t m_n_classes = 0;
};

// Quotient the survivors. They are pairwise non-dominating by construction, so
// a mutual implication here is an equivalence and nothing else, and the sweep
// is over the survivors alone rather than the whole input.
Quotient quotient_by_equivalence(
    const std::vector<tlsf::Specification>& maximal,
    SatisfiabilityChecker& checker) {
    Quotient out;
    out.m_class_of.assign(maximal.size(), 0);
    for (std::size_t i = 0; i < maximal.size(); ++i) {
        bool placed = false;
        for (std::size_t j = 0; j < i && !placed; ++j) {
            if (tlsf_spec_implies(maximal[i], maximal[j], checker)
                    .value_or(false) &&
                tlsf_spec_implies(maximal[j], maximal[i], checker)
                    .value_or(false)) {
                out.m_class_of[i] = out.m_class_of[j];
                placed = true;
            }
        }
        if (!placed) {
            out.m_class_of[i] = out.m_n_classes++;
        }
    }
    return out;
}

// `members` is indexed by position in the distinct corpus, not by position in
// `maximal`, so every lookup goes through `position_of`.
void print_report(
    const std::vector<tlsf::Specification>& maximal, const Quotient& quotient,
    const std::unordered_map<tlsf::Specification, std::size_t>& position_of,
    const std::vector<std::vector<std::string>>& members,
    std::size_t n_distinct, std::size_t parse_failures) {
    std::size_t n_files = 0;
    for (const std::vector<std::string>& group : members) {
        n_files += group.size();
    }
    std::cout << "files      " << n_files << "\n"
              << "distinct   " << n_distinct << "\n"
              << "maximal    " << maximal.size() << "\n"
              << "classes    " << quotient.m_n_classes << "\n";
    if (parse_failures > 0) {
        std::cout << "unparsed   " << parse_failures << "\n";
    }
    std::cout << "\n";
    for (std::size_t i = 0; i < maximal.size(); ++i) {
        const std::size_t position = position_of.at(maximal[i]);
        std::cout << "class " << quotient.m_class_of[i] << "  "
                  << members[position].front();
        if (members[position].size() > 1) {
            std::cout << "  (+" << members[position].size() - 1
                      << " identical)";
        }
        std::cout << "\n";
    }
}

}  // namespace

int main(int argc, const char* const argv[]) {
    if (argc == 0 || argv == nullptr || argv[0] == nullptr) {
        std::cerr << "fatal: missing argv[0]\n";
        return 1;
    }
    if (handle_info_flags(argc, argv, print_usage)) {
        return 0;
    }
    const std::optional<Args> maybe_args = parse_args(argc, argv);
    if (!maybe_args.has_value()) {
        print_usage(argv[0]);
        return 1;
    }
    const Args& args = *maybe_args;

    const std::vector<std::string> files = expand_paths(args.paths);
    if (files.empty()) {
        std::cerr << "no .tlsf files found\n";
        return 1;
    }

    // Structural duplicates cost nothing to remove and would each pay for a
    // full row of the pairwise sweep, so they are collapsed before any solver
    // call. compute_subsumed does this internally too; doing it here as well is
    // what lets the report name the files behind each survivor.
    std::vector<tlsf::Specification> distinct;
    std::vector<std::vector<std::string>> members;
    std::unordered_map<tlsf::Specification, std::size_t> position_of;
    std::size_t parse_failures = 0;
    for (const std::string& file : files) {
        const std::optional<std::string> contents = read_file_contents(file);
        if (!contents.has_value()) {
            std::cerr << file << ": cannot read file\n";
            ++parse_failures;
            continue;
        }
        tlsf::Specification spec;
        try {
            spec = tlsf::parse(*contents);
        } catch (const std::exception& exc) {
            std::cerr << file << ": " << exc.what() << "\n";
            ++parse_failures;
            continue;
        }
        const auto [iter, inserted] =
            position_of.try_emplace(spec, distinct.size());
        if (inserted) {
            distinct.push_back(std::move(spec));
            members.push_back({file});
        } else {
            members[iter->second].push_back(file);
        }
    }
    if (distinct.empty()) {
        std::cerr << "no specifications parsed\n";
        return 1;
    }
    // Implication between specs over different signal sets is not the relation
    // this reports, so say so rather than printing a number that means nothing.
    for (const tlsf::Specification& spec : distinct) {
        if (spec.m_inputs != distinct.front().m_inputs ||
            spec.m_outputs != distinct.front().m_outputs) {
            std::cerr << "warning: the input set mixes signal alphabets; "
                         "implication across them is not meaningful\n";
            break;
        }
    }

    Config cfg;
    cfg.parallel = args.jobs;
    cfg.black_timeout = std::chrono::milliseconds{args.timeout_s * 1000};
    // Reached through check_satisfiability's simplification step, which decides
    // the query outright whenever it folds to a constant. compare.cpp sizes it
    // at 300 s off amba and documents why anything smaller silently changes the
    // verdict rather than merely losing a simplification.
    cfg.ltlfilt_timeout = std::chrono::milliseconds{300'000};
    apply_tool_timeouts(cfg);
    set_thread_pool_size(cfg.parallel);
    SatisfiabilityChecker& checker = global_sat_checker();

    std::size_t reported = 0;
    const std::vector<tlsf::Specification> maximal =
        // No original specification here -- `maximal` takes a bare directory
        // of TLSF files -- so an equivalence class collapses on operator< with
        // no similarity to rank it.
        tlsf_make_implication_filter(
            checker, nullptr, [&reported](std::size_t done, std::size_t total) {
                reported = done;
                if (done % 500 == 0 || done == total) {
                    std::cerr << "\r  pairs " << done << "/" << total
                              << std::flush;
                }
            })(distinct);
    if (reported > 0) {
        std::cerr << "\n";
    }

    const Quotient quotient = quotient_by_equivalence(maximal, checker);
    print_report(maximal, quotient, position_of, members, distinct.size(),
                 parse_failures);
    return 0;
}
