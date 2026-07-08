#include "runner/formaliser.hpp"

#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

std::string formaliser_script_path() {
#ifdef FORMALISER_SCRIPT_PATH
    const std::string path = FORMALISER_SCRIPT_PATH;
    // Checked unconditionally (not assert()) since FORMALISER_SCRIPT_PATH is
    // a machine-local, currently-temporary path: it can go stale between
    // configure time and a run without CMake ever re-running, and assert()
    // is a no-op in the NDEBUG release/relwithdebinfo builds, which would
    // otherwise spawn `node` on a missing script and hang rather than fail.
    if (access(path.c_str(), F_OK) != 0) {
        throw std::runtime_error("formaliser script not found: " + path);
    }
    return path;
#else
    assert(false);
    return "";
#endif
}

std::vector<std::string> formaliser_command() {
    return {"node",      formaliser_script_path(),
            "formalize", "--logic",
            "ft-inf",    "--batch"};
}

PersistentProcess::PersistentProcess(std::vector<std::string> command)
    : m_command(std::move(command)) {}

PersistentProcess::~PersistentProcess() {
    if (!m_spawned) {
        return;
    }
    // Closing the write end sends EOF on the child's stdin, which a
    // well-behaved --batch CLI treats as "no more requests" and exits on.
    close(m_write_fd);
    int wait_status = 0;
    struct rusage child_usage{};
    [[maybe_unused]] const pid_t waited =
        wait4(m_pid, &wait_status, 0, &child_usage);
    assert(waited >= 0);
    // Sampled once here rather than per request: the child is long-lived, so
    // this is its whole-run user+sys CPU across every formalise() call.
    RequirementFormaliser::total_cpu_s +=
        static_cast<double>(child_usage.ru_utime.tv_sec) +
        (static_cast<double>(child_usage.ru_utime.tv_usec) / 1e6) +
        static_cast<double>(child_usage.ru_stime.tv_sec) +
        (static_cast<double>(child_usage.ru_stime.tv_usec) / 1e6);
    close(m_read_fd);
}

void PersistentProcess::ensure_spawned() {
    if (m_spawned) {
        return;
    }
    // Build argv before forking: heap allocation inside the child between
    // fork() and execvp() can deadlock if another thread held the allocator
    // lock at the moment of the fork (e.g. under ASAN's allocator).
    std::vector<char*> argv(m_command.size() + 1);
    for (std::size_t arg_idx = 0; arg_idx < m_command.size(); ++arg_idx) {
        argv[arg_idx] = const_cast<char*>(m_command[arg_idx].c_str());
    }
    argv[m_command.size()] = nullptr;

    std::array<int, 2> stdin_pipe = {-1, -1};
    std::array<int, 2> stdout_pipe = {-1, -1};
    [[maybe_unused]] const int stdin_pipe_result = pipe(stdin_pipe.data());
    assert(stdin_pipe_result == 0);
    [[maybe_unused]] const int stdout_pipe_result = pipe(stdout_pipe.data());
    assert(stdout_pipe_result == 0);

    const pid_t child_pid = fork();
    if (child_pid < 0) {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        assert(false);
        __builtin_unreachable();
    }
    if (child_pid == 0) {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        if (dup2(stdin_pipe[0], STDIN_FILENO) < 0 ||
            dup2(stdout_pipe[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        // execvp (not execv): `node` is a general system interpreter looked
        // up on PATH, not a tool CMake resolves to an absolute path at build
        // time like ltl2tgba/ganak/black.
        execvp(m_command[0].c_str(), argv.data());
        _exit(127);
    }
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    m_pid = child_pid;
    m_write_fd = stdin_pipe[1];
    m_read_fd = stdout_pipe[0];
    m_spawned = true;
}

namespace {

void write_all(int write_fd, const std::string& data) {
    std::size_t written = 0;
    while (written < data.size()) {
        const ssize_t bytes_written =
            write(write_fd, data.data() + written, data.size() - written);
        if (bytes_written > 0) {
            written += static_cast<std::size_t>(bytes_written);
            continue;
        }
        if (bytes_written < 0 && errno == EINTR) {
            continue;
        }
        throw std::runtime_error("failed to write to formaliser process stdin");
    }
}

}  // namespace

std::string PersistentProcess::request(const std::string& line) {
    assert(line.find('\n') == std::string::npos);
    ensure_spawned();
    write_all(m_write_fd, line + "\n");

    // Buffered line read: a single read() may return more or less than one
    // full line, so leftover bytes past the first '\n' are kept for the
    // next call.
    while (true) {
        const auto newline_pos = m_read_buffer.find('\n');
        if (newline_pos != std::string::npos) {
            std::string result = m_read_buffer.substr(0, newline_pos);
            m_read_buffer.erase(0, newline_pos + 1);
            return result;
        }
        std::array<char, 4096> read_buf{};
        // Blocking read while m_proc_mutex (held by the caller) is locked is
        // intentional: the child's stdin/stdout are a single ordered
        // channel, so a response must be fully read before another caller's
        // request can be written, or replies could be attributed to the
        // wrong caller.
        const ssize_t bytes_read =
            read(  // NOLINT(clang-analyzer-unix.BlockInCriticalSection)
                m_read_fd, read_buf.data(), read_buf.size());
        if (bytes_read > 0) {
            m_read_buffer.append(read_buf.data(),
                                 static_cast<std::size_t>(bytes_read));
            continue;
        }
        if (bytes_read == 0) {
            throw std::runtime_error(
                "formaliser process closed stdout before responding");
        }
        if (errno == EINTR) {
            continue;
        }
        throw std::runtime_error("failed to read from formaliser process");
    }
}

RequirementFormaliser::RequirementFormaliser(std::vector<std::string> command)
    : m_proc(std::move(command)) {}

std::string RequirementFormaliser::formalise(
    const std::string& requirement_text) {
    {
        std::scoped_lock lock(m_cache_mutex);
        const auto found = m_cache.find(requirement_text);
        if (found != m_cache.end()) {
            n_cache_hits++;
            return found->second;
        }
        n_cache_misses++;
    }
    const auto start = std::chrono::steady_clock::now();
    std::string ltl;
    {
        std::scoped_lock lock(m_proc_mutex);
        ltl = m_proc.request(requirement_text);
    }
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    std::scoped_lock lock(m_cache_mutex);
    total_time_s += elapsed;
    m_cache.emplace(requirement_text, ltl);
    return ltl;
}

RequirementFormaliser& global_formaliser() {
    static RequirementFormaliser instance(formaliser_command());
    return instance;
}
