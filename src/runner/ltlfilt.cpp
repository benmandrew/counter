#include "runner/ltlfilt.hpp"

#include <poll.h>
#include <spawn.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

#include <array>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "profile.hpp"
#include "runner/spot.hpp"

namespace {

struct ProcessResult {
    int m_exit_code = 0;
    std::string m_output;
    double m_cpu_s = 0.0;
};

double rusage_cpu_seconds(const struct rusage& usage) {
    const double user_s = static_cast<double>(usage.ru_utime.tv_sec) +
                          (static_cast<double>(usage.ru_utime.tv_usec) / 1e6);
    const double sys_s = static_cast<double>(usage.ru_stime.tv_sec) +
                         (static_cast<double>(usage.ru_stime.tv_usec) / 1e6);
    return user_s + sys_s;
}

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
    pid_t child_pid = -1;
    {
        // posix_spawn, not fork: glibc implements it with
        // clone(CLONE_VM|CLONE_VFORK), which neither copies the page tables nor
        // write-protects the parent's address space. fork() does both, and the
        // scoring pool writes from every worker thread immediately afterwards,
        // so each fork triggered a copy-on-write storm across the whole working
        // set -- measured at ~3.5k minor faults per spawn, the dominant term in
        // this process's system time. Nothing here needs a real child address
        // space: the child only redirects its fds and execs.
        COUNTER_PROFILE_SCOPE("proc/fork+exec");
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
        posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDERR_FILENO);
        posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);
        const int spawn_result =
            posix_spawn(&child_pid, arguments[0].c_str(), &actions, nullptr,
                        argv.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
        if (spawn_result != 0) {
            close(pipe_fds[0]);
            close(pipe_fds[1]);
            assert(false);
            __builtin_unreachable();
        }
    }
    close(pipe_fds[1]);
    std::string output;
    {
        COUNTER_PROFILE_SCOPE("proc/read");
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
    }
    close(pipe_fds[0]);
    int wait_status = 0;
    struct rusage child_usage{};
    {
        COUNTER_PROFILE_SCOPE("proc/wait");
        [[maybe_unused]] const pid_t waited =
            wait4(child_pid, &wait_status, 0, &child_usage);
        assert(waited >= 0);
    }
    int exit_code = -1;
    if (WIFEXITED(wait_status)) {
        exit_code = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        exit_code = 128 + WTERMSIG(wait_status);
    }
    return {exit_code, output, rusage_cpu_seconds(child_usage)};
}

// One long-lived `ltlfilt --simplify --skip-errors -F -` child, driven one line
// in / one line out.
//
// Simplification is by far the most frequent tool call in a run (2165 execs on
// a gen20/pop1000 fsm run), and each exec costs a fixed ~9ms and ~2700 minor
// page faults demand-paging ltlfilt and libspot -- independent of how hard the
// formula is, and dwarfing the simplification itself, which is microseconds.
// Keeping the process alive pays that once instead of per formula.
//
// --skip-errors is what makes the protocol safe rather than merely fast: it
// makes ltlfilt echo an unparseable line back verbatim instead of dropping it,
// so the stream stays exactly one response per request. Without it a single
// rejected formula would shift every later response by one and silently return
// another formula's simplification -- a correctness bug, not a slow path. The
// echoed line is also precisely the "return the input unchanged" fallback the
// one-shot path used on failure.
class LtlfiltProcess {
   public:
    // Returns std::nullopt when the child cannot be spawned or has died, so the
    // caller can fall back to a one-shot exec rather than lose the query.
    std::optional<std::string> simplify(const std::string& formula) {
        const std::scoped_lock lock(m_mutex);
        if (m_failed || !ensure_spawned()) {
            return std::nullopt;
        }
        const std::string line = formula + "\n";
        if (!write_all(line)) {
            m_failed = true;
            return std::nullopt;
        }
        return read_line();
    }

    ~LtlfiltProcess() {
        if (!m_spawned) {
            return;
        }
        // EOF on stdin is ltlfilt's cue to finish and exit.
        close(m_write_fd);
        int wait_status = 0;
        struct rusage child_usage{};
        [[maybe_unused]] const pid_t waited =
            wait4(m_pid, &wait_status, 0, &child_usage);
        // Sampled once, not per request: the child is long-lived, so this is
        // its whole-run CPU across every simplify() it served.
        LtlfiltStats::total_cpu_s += rusage_cpu_seconds(child_usage);
        close(m_read_fd);
    }

    LtlfiltProcess() = default;
    LtlfiltProcess(const LtlfiltProcess&) = delete;
    LtlfiltProcess& operator=(const LtlfiltProcess&) = delete;

   private:
    bool ensure_spawned() {
        if (m_spawned) {
            return true;
        }
        const std::string binary = ltlfilt_path();
        if (access(binary.c_str(), F_OK) != 0) {
            m_failed = true;
            return false;
        }
        std::array<int, 2> in_pipe = {-1, -1};
        std::array<int, 2> out_pipe = {-1, -1};
        if (pipe(in_pipe.data()) != 0 || pipe(out_pipe.data()) != 0) {
            m_failed = true;
            return false;
        }
        const std::array<std::string, 5> args = {
            binary, "--simplify", "--skip-errors", "-F", "-"};
        std::array<char*, 6> argv{};
        for (std::size_t i = 0; i < args.size(); ++i) {
            argv[i] = const_cast<char*>(args[i].c_str());
        }
        argv[args.size()] = nullptr;

        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addclose(&actions, in_pipe[1]);
        posix_spawn_file_actions_addclose(&actions, out_pipe[0]);
        posix_spawn_file_actions_adddup2(&actions, in_pipe[0], STDIN_FILENO);
        posix_spawn_file_actions_adddup2(&actions, out_pipe[1], STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&actions, in_pipe[0]);
        posix_spawn_file_actions_addclose(&actions, out_pipe[1]);
        const int spawn_result = posix_spawn(&m_pid, binary.c_str(), &actions,
                                             nullptr, argv.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
        close(in_pipe[0]);
        close(out_pipe[1]);
        if (spawn_result != 0) {
            close(in_pipe[1]);
            close(out_pipe[0]);
            m_failed = true;
            return false;
        }
        m_write_fd = in_pipe[1];
        m_read_fd = out_pipe[0];
        m_spawned = true;
        return true;
    }

    bool write_all(const std::string& data) {
        std::size_t written = 0;
        while (written < data.size()) {
            const ssize_t n =
                write(m_write_fd, data.data() + written, data.size() - written);
            if (n > 0) {
                written += static_cast<std::size_t>(n);
                continue;
            }
            if (n < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }
        return true;
    }

    // A read() may return part of a line or several, so bytes past the first
    // newline are kept for the next request.
    //
    // Bounded by a deadline because "one line in, one line out" is ltlfilt's
    // behaviour, not a guarantee it makes: a blank line, for instance, is
    // consumed silently and answered with nothing, which without a deadline
    // parks this thread on a reply that will never come while it holds
    // m_mutex. Any silent input is therefore a protocol desync, so the worker
    // is retired rather than reused -- its stream position can no longer be
    // trusted to line requests up with replies.
    std::optional<std::string> read_line() {
        const auto deadline =
            std::chrono::steady_clock::now() + k_response_timeout;
        while (true) {
            const auto newline = m_read_buffer.find('\n');
            if (newline != std::string::npos) {
                std::string result = m_read_buffer.substr(0, newline);
                m_read_buffer.erase(0, newline + 1);
                return result;
            }
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now())
                    .count();
            if (remaining <= 0) {
                m_failed = true;
                return std::nullopt;
            }
            struct pollfd pfd{m_read_fd, POLLIN, 0};
            const int ready = poll(&pfd, 1, static_cast<int>(remaining));
            if (ready < 0 && errno == EINTR) {
                continue;
            }
            if (ready <= 0) {
                m_failed = true;
                return std::nullopt;
            }
            std::array<char, 4096> buf{};
            const ssize_t n = read(m_read_fd, buf.data(), buf.size());
            if (n > 0) {
                m_read_buffer.append(buf.data(), static_cast<std::size_t>(n));
                continue;
            }
            if (n < 0 && errno == EINTR) {
                continue;
            }
            // EOF or error: the child is gone and the stream position is no
            // longer trustworthy, so refuse every later request rather than
            // risk answering one query with another's result.
            m_failed = true;
            return std::nullopt;
        }
    }

    // Generous: it exists to break a desync, not to bound a slow
    // simplification. A served request answers in microseconds.
    static constexpr std::chrono::seconds k_response_timeout{10};

    std::mutex m_mutex;
    std::string m_read_buffer;
    pid_t m_pid = -1;
    int m_write_fd = -1;
    int m_read_fd = -1;
    bool m_spawned = false;
    bool m_failed = false;
};

// A pool rather than one process: requests are serialised per child, and
// scoring runs on every pool worker at once, so a single child would turn the
// parallel simplify calls into a queue. Each child is ~13MB resident, so the
// cap keeps the whole pool well under the memory a single ltlsynt query uses.
constexpr std::size_t k_ltlfilt_pool_size = 8;

std::array<LtlfiltProcess, k_ltlfilt_pool_size>& ltlfilt_pool() {
    static std::array<LtlfiltProcess, k_ltlfilt_pool_size> pool;
    return pool;
}

// Spreads callers across the pool without a shared counter on the hot path.
std::size_t pool_slot() {
    static std::atomic<std::size_t> next{0};
    thread_local const std::size_t slot =
        next.fetch_add(1, std::memory_order_relaxed);
    return slot % k_ltlfilt_pool_size;
}

}  // namespace

std::string ltlfilt_path() { return spot_bin_dir() + "/ltlfilt"; }

std::string simplify_ltl(const std::string& formula) {
    COUNTER_PROFILE_SCOPE("ltlfilt/simplify_ltl");
    static std::unordered_map<std::string, std::string> cache;
    static std::mutex cache_mutex;
    {
        COUNTER_PROFILE_SCOPE("ltlfilt/simplify_ltl:cache-lookup");
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
    std::string simplified = formula;
    // Two inputs the line protocol cannot carry, both routed to the one-shot
    // exec instead. A formula spanning lines would be read as several requests.
    // A blank one is worse: ltlfilt consumes it and answers with nothing, which
    // desyncs every later reply on that worker -- the specification formula of
    // a guarantee-free candidate is exactly that, so this is reachable, not
    // hypothetical.
    const bool line_safe =
        formula.find('\n') == std::string::npos &&
        formula.find_first_not_of(" \t\r") != std::string::npos;
    std::optional<std::string> streamed;
    if (line_safe) {
        COUNTER_PROFILE_SCOPE("ltlfilt/persistent-request");
        streamed = ltlfilt_pool()[pool_slot()].simplify(formula);
    }
    double child_cpu_s = 0.0;
    if (streamed.has_value()) {
        simplified = *streamed;
    } else {
        COUNTER_PROFILE_SCOPE("ltlfilt/one-shot-exec");
        const ProcessResult result =
            execute_and_capture({binary, "--simplify", "-f", formula});
        child_cpu_s = result.m_cpu_s;
        if (result.m_exit_code == 0 && !result.m_output.empty()) {
            simplified = result.m_output;
            while (!simplified.empty() && simplified.back() == '\n') {
                simplified.pop_back();
            }
        }
    }
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    std::scoped_lock lock(cache_mutex);
    LtlfiltStats::total_time_s += elapsed;
    LtlfiltStats::total_cpu_s += child_cpu_s;
    cache.emplace(formula, simplified);
    return simplified;
}

std::string normalize_ltl(const std::string& formula) {
    std::string simplified = simplify_ltl(formula);
    // SPOT uses "0"/"1" for the boolean constants false/true. There is no
    // single keyword accepted by all downstream tools (black treats "false" as
    // an atom, not a constant), so fall back to the original formula in these
    // cases to preserve correctness.
    if (simplified == "0" || simplified == "1") {
        return formula;
    }
    return simplified;
}

bool ltl_equivalent(const std::string& lhs, const std::string& rhs) {
    const std::string binary = ltlfilt_path();
    if (access(binary.c_str(), F_OK) != 0) {
        return true;
    }
    const ProcessResult result =
        execute_and_capture({binary, "--equivalent-to=" + rhs, "-f", lhs});
    // ltlfilt's filter convention: exit 0 means the input formula (lhs)
    // matched (i.e. is equivalent to rhs); exit 1 means it didn't. Any other
    // status (parse error, crash) is inconclusive, not a mismatch.
    if (result.m_exit_code == 0) {
        return true;
    }
    if (result.m_exit_code == 1) {
        return false;
    }
    return true;
}
