#include "runner/ltlfilt.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "runner/spot.hpp"

namespace {

struct ProcessResult {
    int m_exit_code = 0;
    std::string m_output;
};

ProcessResult execute_and_capture(const std::vector<std::string>& arguments) {
    assert(!arguments.empty());
    // Build argv before forking: heap allocation inside the child between
    // fork() and execv() can deadlock if another thread held the allocator
    // lock at the moment of the fork (e.g. under ASAN's allocator).
    std::vector<char*> argv(arguments.size() + 1);
    for (std::size_t arg_idx = 0; arg_idx < arguments.size(); ++arg_idx) {
        argv[arg_idx] = const_cast<char*>(arguments[arg_idx].c_str());
    }
    argv[arguments.size()] = nullptr;
    std::array<int, 2> pipe_fds = {-1, -1};
    [[maybe_unused]] const int pipe_result = pipe(pipe_fds.data());
    assert(pipe_result == 0);
    const pid_t child_pid = fork();
    if (child_pid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        assert(false);
        __builtin_unreachable();
    }
    if (child_pid == 0) {
        close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0 ||
            dup2(pipe_fds[1], STDERR_FILENO) < 0) {
            _exit(127);
        }
        close(pipe_fds[1]);
        execv(arguments[0].c_str(), argv.data());
        _exit(127);
    }
    close(pipe_fds[1]);
    std::string output;
    std::array<char, 4096> read_buf{};
    while (true) {
        const ssize_t bytes_read =
            read(pipe_fds[0], read_buf.data(), read_buf.size());
        if (bytes_read > 0) {
            output.append(read_buf.data(),
                          static_cast<std::size_t>(bytes_read));
            continue;
        }
        if (bytes_read == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        close(pipe_fds[0]);
        assert(false);
        __builtin_unreachable();
    }
    close(pipe_fds[0]);
    int wait_status = 0;
    [[maybe_unused]] const pid_t waited = waitpid(child_pid, &wait_status, 0);
    assert(waited >= 0);
    int exit_code = -1;
    if (WIFEXITED(wait_status)) {
        exit_code = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        exit_code = 128 + WTERMSIG(wait_status);
    }
    return {exit_code, output};
}

}  // namespace

std::string ltlfilt_path() { return spot_bin_dir() + "/ltlfilt"; }

std::string normalize_ltl(const std::string& formula) {
    static std::unordered_map<std::string, std::string> cache;
    static std::mutex cache_mutex;
    {
        std::scoped_lock lock(cache_mutex);
        const auto found = cache.find(formula);
        if (found != cache.end()) {
            LtlfiltStats::n_cache_hits++;
            return found->second;
        }
        LtlfiltStats::n_cache_misses++;
    }
    const std::string binary = ltlfilt_path();
    if (access(binary.c_str(), F_OK) != 0) {
        std::scoped_lock lock(cache_mutex);
        cache.emplace(formula, formula);
        return formula;
    }
    const auto start = std::chrono::steady_clock::now();
    const ProcessResult result =
        execute_and_capture({binary, "--simplify", "-f", formula});
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    std::string normalized = formula;
    if (result.m_exit_code == 0 && !result.m_output.empty()) {
        normalized = result.m_output;
        while (!normalized.empty() && normalized.back() == '\n') {
            normalized.pop_back();
        }
        // SPOT uses "0"/"1" for the boolean constants false/true. There is no
        // single keyword accepted by all downstream tools (black treats
        // "false" as an atom, not a constant), so fall back to the original
        // formula in these cases to preserve correctness.
        if (normalized == "0" || normalized == "1") {
            normalized = formula;
        }
    }
    std::scoped_lock lock(cache_mutex);
    LtlfiltStats::total_time_s += elapsed;
    cache.emplace(formula, normalized);
    return normalized;
}
