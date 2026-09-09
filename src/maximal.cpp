#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "config.hpp"
#include "driver_support.hpp"
#include "fingerprint/lasso.hpp"
#include "runner/black.hpp"
#include "thread_pool.hpp"
#include "tlsf/antichain.hpp"
#include "tlsf/filter.hpp"
#include "tlsf/fingerprint_prefilter.hpp"
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
    std::string curve_index;
    std::size_t jobs{0};
    std::size_t wave{0};
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
        << "  --curve F    Walk the accumulator index F in timestamp order\n"
           "               and print the antichain's event log instead, one\n"
           "               row per admission, drop and removal. Specification\n"
           "               files resolve against F's own directory.\n"
        << "  --jobs N     Solver calls in flight (default: hardware "
           "concurrency).\n"
        << "  --wave W     Arrivals scanned concurrently under --curve\n"
           "               (default: twice the pool; 1 walks serially).\n"
        << "  --timeout S  Per-black-call budget in seconds (default: 20).\n"
        << "  --version    Print the git commit this binary was built from.\n";
}

// Hand-rolled rather than through find_unknown_arg and collect_argument_paths,
// which between them assume a driver whose paths are all flag values: the first
// reports every positional as unknown and the second collects the flags as
// paths. This one takes a variable number of positional arguments, so it walks
// argv once and still refuses an unrecognised flag rather than ignoring it.
// Consumes the value of a flag taking one, or reports that @p arg is not one of
// them. Split out of parse_args, which the cognitive-complexity check rejects
// once the flag table grows past a couple of entries.
enum class FlagStatus : std::uint8_t { NotAFlag, Consumed, Bad };

FlagStatus take_valued_flag(const std::string& arg, int& index, int argc,
                            const char* const* argv, Args& args) {
    static const std::array<const char*, 4> k_valued = {"--jobs", "--timeout",
                                                        "--wave", "--curve"};
    if (std::find_if(k_valued.begin(), k_valued.end(),
                     [&arg](const char* name) { return arg == name; }) ==
        k_valued.end()) {
        return FlagStatus::NotAFlag;
    }
    if (index + 1 >= argc || argv[index + 1] == nullptr) {
        std::cerr << arg << " expects a value\n";
        return FlagStatus::Bad;
    }
    const std::string value(argv[++index]);
    if (arg == "--curve") {
        args.curve_index = value;
        return FlagStatus::Consumed;
    }
    const std::optional<std::size_t> count = parse_seed(value);
    if (!count.has_value() || *count == 0) {
        std::cerr << arg << " expects a positive integer\n";
        return FlagStatus::Bad;
    }
    if (arg == "--jobs") {
        args.jobs = *count;
    } else if (arg == "--wave") {
        args.wave = *count;
    } else {
        args.timeout_s = static_cast<std::int64_t>(*count);
    }
    return FlagStatus::Consumed;
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
        const FlagStatus status = take_valued_flag(arg, i, argc, argv, args);
        if (status == FlagStatus::Bad) {
            return std::nullopt;
        }
        if (status == FlagStatus::Consumed) {
            continue;
        }
        if (arg.rfind("--", 0) == 0) {
            std::cerr << "unknown argument: " << arg << "\n";
            return std::nullopt;
        }
        args.paths.push_back(arg);
    }
    // Curve mode takes its files from the index, so a positional there names a
    // set nothing reads; the batch mode has nothing to read without one.
    if (args.curve_index.empty() == args.paths.empty()) {
        std::cerr << (args.curve_index.empty()
                          ? "expected a directory or file, or --curve\n"
                          : "--curve takes its files from the index; drop the "
                            "positional argument\n");
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
//
// This loop is serial and used to be free: every query it asks, the pairwise
// sweep had already asked and cached. The sweep's fingerprint prefilter took
// that away, since the pairs reaching here are the mutually non-implying ones
// and those are exactly the ones a sampled word refutes, so the sweep now
// skips them and this loop pays a fresh subprocess for each. Measured over 80
// candidates that was 1030 serial ltlfilt calls and 20.4s of a 21s run. The
// same prefilter applies here for the same reason and restores it: a word one
// survivor accepts and another rejects rules out equivalence outright.
Quotient quotient_by_equivalence(
    const std::vector<tlsf::Specification>& maximal,
    SatisfiabilityChecker& checker,
    const std::vector<fingerprint::PackedFingerprint>& prints) {
    Quotient out;
    out.m_class_of.assign(maximal.size(), 0);
    const bool have_prints = prints.size() == maximal.size();
    for (std::size_t i = 0; i < maximal.size(); ++i) {
        bool placed = false;
        for (std::size_t j = 0; j < i && !placed; ++j) {
            if (have_prints &&
                (fingerprint::refutes_implication(prints[i], prints[j]) ||
                 fingerprint::refutes_implication(prints[j], prints[i]))) {
                continue;
            }
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

// One row of the accumulator's index.tsv: which file, and how many seconds into
// the search the run passed it through the output gate. The generation column
// sits between them and is not read here, the walk being over time.
std::optional<std::vector<std::pair<std::string, double>>> read_index(
    const std::string& path) {
    std::ifstream index(path);
    if (!index) {
        std::cerr << path << ": cannot read index\n";
        return std::nullopt;
    }
    std::vector<std::pair<std::string, double>> rows;
    std::string line;
    while (std::getline(index, line)) {
        const std::size_t first = line.find('\t');
        if (first == std::string::npos) {
            continue;
        }
        const std::size_t second = line.find('\t', first + 1);
        if (second == std::string::npos) {
            // A run killed mid-append leaves a partial last row. Dropping it
            // costs one candidate and keeps every row already flushed, which
            // is the whole reason the index is written a row at a time.
            continue;
        }
        const std::string name = line.substr(0, first);
        double elapsed = 0.0;
        try {
            elapsed = std::stod(line.substr(second + 1));
        } catch (const std::exception&) {
            continue;  // The header, and any row whose time did not land.
        }
        rows.emplace_back(name, elapsed);
    }
    return rows;
}

const char* event_name(tlsf::AntichainEvent event) {
    switch (event) {
        case tlsf::AntichainEvent::Admit:
            return "admit";
        case tlsf::AntichainEvent::Drop:
            return "drop";
        case tlsf::AntichainEvent::Remove:
            break;
    }
    return "remove";
}

// Walks the index in timestamp order and prints the antichain's event log.
//
// Every prefix's maximal set is recoverable from it: an admission adds, a
// removal takes away, and both are permanent, so membership at any instant is
// the admissions up to it less the removals up to it. That is the whole reason
// this replaces one `maximal` process per time cut -- the cuts were re-deciding
// pairs the previous cut had already decided, in a fresh process with a cold
// solver cache each time.
int run_curve(const Args& args, SatisfiabilityChecker& checker) {
    const std::optional<std::vector<std::pair<std::string, double>>> index =
        read_index(args.curve_index);
    if (!index.has_value()) {
        return 1;
    }
    const std::filesystem::path directory =
        std::filesystem::path(args.curve_index).parent_path();
    std::vector<tlsf::Arrival> arrivals;
    arrivals.reserve(index->size());
    std::size_t parse_failures = 0;
    for (const auto& [name, elapsed] : *index) {
        const std::optional<std::string> contents =
            read_file_contents((directory / name).string());
        if (!contents.has_value()) {
            ++parse_failures;
            continue;
        }
        try {
            arrivals.push_back({name, elapsed, tlsf::parse(*contents)});
        } catch (const std::exception& exc) {
            std::cerr << name << ": " << exc.what() << "\n";
            ++parse_failures;
        }
    }
    if (arrivals.empty()) {
        std::cerr << "no specifications parsed\n";
        return 1;
    }
    // Stable, so candidates sharing a timestamp keep the order the run found
    // them in and the walk stays a function of the index alone.
    std::stable_sort(arrivals.begin(), arrivals.end(),
                     [](const tlsf::Arrival& left, const tlsf::Arrival& right) {
                         return left.m_elapsed_s < right.m_elapsed_s;
                     });

    // Written as the walk produces it rather than at the end, so a caller that
    // kills this on a deadline keeps the prefix. That is what the per-cut loop
    // this replaces already had: the early cuts of a log-spaced curve are where
    // its information is, and a hard run should yield a short curve rather than
    // none.
    tlsf::AntichainStats stats;
    std::cout << "elapsed_s\tfile\tevent\tn_maximal\n";
    const std::vector<tlsf::AntichainRow> rows = tlsf::running_antichain(
        arrivals, checker, nullptr, args.wave,
        [](std::size_t done, std::size_t total) {
            std::cerr << "\r  arrivals " << done << "/" << total << std::flush;
            std::cout << std::flush;
        },
        [](const tlsf::AntichainRow& row) {
            std::cout << std::fixed << std::setprecision(6) << row.m_elapsed_s
                      << "\t" << row.m_name << "\t" << event_name(row.m_event)
                      << "\t" << row.m_size << "\n";
        },
        &stats);
    std::cout << std::flush;
    std::cerr << "\n";
    std::cerr
        << "arrivals   " << arrivals.size() << "\n"
        << "maximal    "
        << tlsf::antichain_members_at(rows, arrivals.back().m_elapsed_s).size()
        << "\n"
        << "queries    " << stats.m_solver_queries << "\n"
        << "refuted    " << stats.m_refuted_directions << "\n"
        << "shortcut   " << stats.m_short_circuited << "\n"
        << "reconciled " << stats.m_reconciled_pairs << "\n";
    if (parse_failures > 0) {
        std::cerr << "unparsed   " << parse_failures << "\n";
    }
    return 0;
}

// The two settings are measured over this tool's own queries, whole-spec
// implications of 1000-1600 characters. `ltlfilt --simplify` took 95.6% of
// solver wall time (1153.7s against 52.9s for the decision itself) and a
// 40-file batch went from 304s to 30s without it, with the same survivors;
// without the pass the unsimplified query runs about 2x longer, so the 500ms
// SPOT budget tuned for the search tips over under load and an undecided
// `ExpectUnsat` query keeps both sides. Giving SPOT black's budget instead
// restored agreement on 264 of 264 cut-values across a 10-run sample.
SatisfiabilityChecker& configure_checker(const Args& args) {
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
    checker.set_simplify(false);
    checker.set_spot_budget(cfg.black_timeout);
    return checker;
}

int run_batch(const Args& args, SatisfiabilityChecker& checker) {
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

    const Quotient quotient = quotient_by_equivalence(
        maximal, checker, tlsf::prefilter::fingerprints_of(maximal));
    print_report(maximal, quotient, position_of, members, distinct.size(),
                 parse_failures);
    return 0;
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
    SatisfiabilityChecker& checker = configure_checker(*maybe_args);
    return maybe_args->curve_index.empty() ? run_batch(*maybe_args, checker)
                                           : run_curve(*maybe_args, checker);
}
