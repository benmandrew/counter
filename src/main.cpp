#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cpptrace/cpptrace.hpp>

#include "config.hpp"
#include "filter/implication.hpp"
#include "fitness/semantic_similarity.hpp"
#include "fitness/status.hpp"
#include "fitness/syntactic_similarity.hpp"
#include "genetic/generation.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "runner/ganak.hpp"
#include "runner/spot.hpp"

namespace {

constexpr std::size_t k_path_buffer_size = 4096;
constexpr std::size_t k_num_buffer_size = 32;
constexpr std::size_t k_frame_buffer_size = 100;

std::array<char, k_path_buffer_size> g_tracer_path = {};
std::array<char, k_path_buffer_size> g_crash_dir = {};

void copy_path_to_buffer(std::array<char, k_path_buffer_size>& destination,
                         const std::filesystem::path& path) {
    const std::string value = path.string();
    if (value.size() + 1 > destination.size()) {
        throw std::runtime_error("path exceeds crash handler buffer size");
    }
    std::memcpy(destination.data(), value.c_str(), value.size() + 1);
}

std::size_t format_unsigned(char* destination, std::size_t destination_size,
                            std::size_t value) {
    std::array<char, k_num_buffer_size> tmp = {};
    std::size_t length = 0;
    do {
        tmp[length++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value > 0 && length < tmp.size());
    if (length + 1 > destination_size) {
        return 0;
    }
    for (std::size_t index = 0; index < length; ++index) {
        destination[index] = tmp[length - 1 - index];
    }
    destination[length] = '\0';
    return length;
}

bool write_all(int file_fd, const void* data, std::size_t size) {
    const char* bytes = static_cast<const char*>(data);
    std::size_t written = 0;
    while (written < size) {
        const ssize_t result = write(file_fd, bytes + written, size - written);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        written += static_cast<std::size_t>(result);
    }
    return true;
}

void warmup_cpptrace() {
    std::array<cpptrace::frame_ptr, 10> buf = {};
    const std::size_t frame_count =
        cpptrace::safe_generate_raw_trace(buf.data(), buf.size());
    if (frame_count > 0) {
        cpptrace::safe_object_frame frame{};
        cpptrace::get_safe_object_frame(buf[0], &frame);
    }
}

void crash_handler(int signo, [[maybe_unused]] siginfo_t* siginfo,
                   [[maybe_unused]] void* context) {
    std::array<cpptrace::frame_ptr, k_frame_buffer_size> buf = {};
    const std::size_t frame_count =
        cpptrace::safe_generate_raw_trace(buf.data(), buf.size());

    std::array<char, k_num_buffer_size> pid_buffer = {};
    std::array<char, k_num_buffer_size> signal_buffer = {};
    std::array<char, k_num_buffer_size> timestamp_buffer = {};
    const std::size_t pid_length =
        format_unsigned(pid_buffer.data(), pid_buffer.size(),
                        static_cast<std::size_t>(getpid()));
    const std::size_t signal_length =
        format_unsigned(signal_buffer.data(), signal_buffer.size(),
                        static_cast<std::size_t>(signo));
    timespec now{};
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        now.tv_sec = 0;
    }
    const std::size_t timestamp_length =
        format_unsigned(timestamp_buffer.data(), timestamp_buffer.size(),
                        static_cast<std::size_t>(now.tv_sec));
    if (pid_length == 0 || signal_length == 0 || timestamp_length == 0) {
        _exit(1);
    }

    const std::size_t crash_dir_length = std::strlen(g_crash_dir.data());
    const char* suffix = ".log";
    const char* prefix = "/crash_";
    const char* separator = "_";
    const std::size_t total_length = crash_dir_length + std::strlen(prefix) +
                                     pid_length + std::strlen(separator) +
                                     timestamp_length + std::strlen(suffix);
    std::array<char, k_path_buffer_size> log_path = {};
    if (total_length + 1 > log_path.size()) {
        _exit(1);
    }

    std::size_t cursor = 0;
    std::memcpy(log_path.data() + cursor, g_crash_dir.data(), crash_dir_length);
    cursor += crash_dir_length;
    std::memcpy(log_path.data() + cursor, prefix, std::strlen(prefix));
    cursor += std::strlen(prefix);
    std::memcpy(log_path.data() + cursor, pid_buffer.data(), pid_length);
    cursor += pid_length;
    std::memcpy(log_path.data() + cursor, separator, std::strlen(separator));
    cursor += std::strlen(separator);
    std::memcpy(log_path.data() + cursor, timestamp_buffer.data(),
                timestamp_length);
    cursor += timestamp_length;
    std::memcpy(log_path.data() + cursor, suffix, std::strlen(suffix));
    cursor += std::strlen(suffix);
    log_path[cursor] = '\0';

    std::array<int, 2> pipefd = {-1, -1};
    if (pipe(pipefd.data()) != 0) {
        _exit(1);
    }

    pid_t child_pid = fork();
    if (child_pid == 0) {
        close(pipefd[1]);
        if (dup2(pipefd[0], STDIN_FILENO) < 0) {
            _exit(1);
        }
        close(pipefd[0]);
        execl(g_tracer_path.data(), "signal_tracer", log_path.data(),
              signal_buffer.data(), pid_buffer.data(),
              static_cast<char*>(nullptr));
        _exit(1);
    }
    if (child_pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        _exit(1);
    }

    close(pipefd[0]);
    for (std::size_t index = 0; index < frame_count; ++index) {
        cpptrace::safe_object_frame frame{};
        cpptrace::get_safe_object_frame(buf[index], &frame);
        if (!write_all(pipefd[1], &frame, sizeof(frame))) {
            break;
        }
    }
    close(pipefd[1]);
    waitpid(child_pid, nullptr, 0);
    _exit(1);
}

void init_cpptrace(char* executable_name) {
    const std::filesystem::path executable_path =
        std::filesystem::absolute(std::filesystem::path(executable_name));
    copy_path_to_buffer(g_tracer_path,
                        executable_path.parent_path() / "signal_tracer");

    const std::filesystem::path crash_dir_path =
        std::filesystem::absolute(std::filesystem::path("crashes"));
    std::filesystem::create_directories(crash_dir_path);
    copy_path_to_buffer(g_crash_dir, crash_dir_path);
    warmup_cpptrace();
    cpptrace::register_terminate_handler();
    struct sigaction sig_action{};
    sig_action.sa_sigaction = crash_handler;
    sig_action.sa_flags = SA_SIGINFO;
    sigemptyset(&sig_action.sa_mask);
    sigaction(SIGSEGV, &sig_action, nullptr);
    sigaction(SIGABRT, &sig_action, nullptr);
    sigaction(SIGFPE, &sig_action, nullptr);
}

}  // namespace

AggregateWeightedFitnessFunction get_fitness_function(
    const Specification& original_spec) {
    auto synsim = [original_spec](const Specification& spec) -> double {
        return syntactic_similarity(spec, original_spec);
    };
    auto semsim = [original_spec](const Specification& spec) -> double {
        return semantic_similarity(spec, original_spec);
    };
    auto status = [](const Specification& spec) -> double {
        return specification_status(spec, global_sat_checker(),
                                    global_real_checker());
    };
    return AggregateWeightedFitnessFunction(
        {{synsim, Config::fitness_weight_syntactic},
         {semsim, Config::fitness_weight_semantic},
         {status, Config::fitness_weight_status}});
}

Specification get_spec() {
    std::vector<Requirement> assumptions = {};
    std::vector<Requirement> guarantees = {
        Requirement(Formula("r1"), Formula("g1"), timing::eventually(),
                    "G(r1 -> F g1)"),
        Requirement(Formula("r2"), Formula("g2"), timing::eventually(),
                    "G(r2 -> F g2)"),
        Requirement(Formula("!a"), Formula("!g1 & !g2"), timing::immediately(),
                    "G(!a -> (!g1 & !g2))"),
    };
    std::vector<std::string> in_atoms = {"a", "r1", "r2"};
    std::vector<std::string> out_atoms = {"g1", "g2"};
    return Specification(assumptions, guarantees, in_atoms, out_atoms);
}

void print_timing_report() {
    auto print_row = [](const char* name, std::size_t calls, double total_s,
                        std::size_t cache_hits) {
        const double avg_s =
            calls > 0 ? total_s / static_cast<double>(calls) : 0.0;
        std::cout << std::left << std::setw(12) << name << std::right
                  << std::setw(6) << calls << " calls  " << std::fixed
                  << std::setprecision(3) << std::setw(8) << total_s
                  << "s total  " << std::setw(8) << avg_s << "s avg";
        if (cache_hits > 0) {
            std::cout << "  (+" << cache_hits << " cache hits)";
        }
        std::cout << "\n";
    };
    std::cout << "\nTool timing report:\n";
    print_row("ltl2tgba", Ltl2tgbaStats::n_cache_misses,
              Ltl2tgbaStats::total_time_s, Ltl2tgbaStats::n_cache_hits);
    print_row("ltlsynt", RealizabilityChecker::n_cache_misses,
              RealizabilityChecker::total_time_s,
              RealizabilityChecker::n_cache_hits);
    print_row("black", SatisfiabilityChecker::n_cache_misses,
              SatisfiabilityChecker::total_time_s,
              SatisfiabilityChecker::n_cache_hits);
    print_row("ganak", GanakStats::n_cache_misses, GanakStats::total_time_s,
              GanakStats::n_cache_hits);
    std::cout << "\nFitness cache: "
              << AggregateWeightedFitnessFunction::n_cache_hits << " hits / "
              << AggregateWeightedFitnessFunction::n_cache_misses
              << " misses\n";
}

std::vector<ScoredSpecification> original_population(
    Specification& original_spec,
    const AggregateWeightedFitnessFunction& fitness_function,
    std::size_t population_size) {
    std::vector<ScoredSpecification> population;
    population.reserve(population_size);
    for (std::size_t i = 0; i < population_size; ++i) {
        population.push_back({original_spec, fitness_function(original_spec)});
    }
    return population;
}

int main(int argc, char* argv[]) {
    if (argc == 0 || argv == nullptr || argv[0] == nullptr) {
        std::cerr << "fatal: missing argv[0]\n";
        return 1;
    }
    init_cpptrace(argv[0]);
    Specification original_spec = get_spec();
    std::cout << "Original specification:\n"
              << original_spec.to_string() << "\n";
    // 1. Fitness functions
    AggregateWeightedFitnessFunction fitness_function =
        get_fitness_function(original_spec);
    // 2. Initial population — each requirement wrapped in a specification
    std::vector<ScoredSpecification> population = original_population(
        original_spec, fitness_function, Config::population_size);
    // 3. Random source
    std::random_device rng_dev;
    RandomSource random_source = make_random_source_from_seed(rng_dev());
    // 4. Run genetic algorithm for a few generations
    std::size_t pop_size = population.size();

    for (std::size_t gen_idx = 0; gen_idx < Config::generations; ++gen_idx) {
        const auto start = std::chrono::steady_clock::now();
        auto on_progress = [&](std::size_t done, std::size_t total) {
            const double elapsed = std::chrono::duration<double>(
                                       std::chrono::steady_clock::now() - start)
                                       .count();
            std::cout << "\rGeneration " << std::setw(2) << gen_idx + 1 << ": "
                      << std::setw(3) << (done * 100 / total) << "%  "
                      << std::fixed << std::setprecision(2) << elapsed << "s"
                      << std::flush;
        };
        population = evolve_generation(population, pop_size, fitness_function,
                                       {}, random_source, on_progress);
        const double elapsed = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
        std::cout << "\r\033[KGeneration " << std::setw(2) << gen_idx + 1
                  << ": 100%  " << std::fixed << std::setprecision(2) << elapsed
                  << "s\n";
    }
    // 5. Collect realizable specifications, then filter to maximal under
    // implication
    std::vector<Specification> realizable_vec;
    for (const ScoredSpecification& scored : population) {
        if (specification_status(scored.specification, global_sat_checker(),
                                 global_real_checker()) == 1.0) {
            realizable_vec.push_back(scored.specification);
        }
    }
    const FilterFunction implication_filter =
        make_implication_filter(global_sat_checker());
    const std::vector<Specification> maximal =
        implication_filter(realizable_vec);
    std::cout << "\nRealizable specifications after " << Config::generations
              << " generations (" << maximal.size() << " reduced from "
              << realizable_vec.size() << "):\n";
    for (const Specification& spec : maximal) {
        std::cout << spec.to_string() << "\n\n";
    }
    print_timing_report();
    return 0;
}
