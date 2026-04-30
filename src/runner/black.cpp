#include "runner/black.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

struct ProcessResult {
    int m_exit_code;
    std::string m_output;
};

ProcessResult execute_and_capture(const std::vector<std::string>& arguments) {
    assert(!arguments.empty());
    int pipe_fds[2] = {-1, -1};
    assert(pipe(pipe_fds) == 0);
    const pid_t child_pid = fork();
    if (child_pid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        assert(false);
    }
    if (child_pid == 0) {
        close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0 ||
            dup2(pipe_fds[1], STDERR_FILENO) < 0) {
            _exit(127);
        }
        close(pipe_fds[1]);
        std::unique_ptr<char*[]> argv =
            std::make_unique<char*[]>(arguments.size() + 1);
        for (std::size_t i = 0; i < arguments.size(); ++i) {
            argv[i] = const_cast<char*>(arguments[i].c_str());
        }
        argv[arguments.size()] = nullptr;
        execv(arguments[0].c_str(), argv.get());
        _exit(127);
    }
    close(pipe_fds[1]);
    std::string output;
    char buffer[4096];
    while (true) {
        const ssize_t bytes_read = read(pipe_fds[0], buffer, sizeof(buffer));
        if (bytes_read > 0) {
            output.append(buffer, static_cast<std::size_t>(bytes_read));
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
    }
    close(pipe_fds[0]);
    int wait_status = 0;
    assert(waitpid(child_pid, &wait_status, 0) >= 0);
    int exit_code = -1;
    if (WIFEXITED(wait_status)) {
        exit_code = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        exit_code = 128 + WTERMSIG(wait_status);
    }
    return {exit_code, output};
}

}  // namespace

std::string black_executable_path() {
#ifdef BLACK_EXECUTABLE_PATH
    return BLACK_EXECUTABLE_PATH;
#else
    assert(false);
    return "";
#endif
}

bool SatisfiabilityChecker::check_satisfiability(
    const std::string& ltl_formula) {
    const auto it = m_cache.find(ltl_formula);
    if (it != m_cache.end()) {
        return it->second;
    }
    const std::string black = black_executable_path();
    assert(access(black.c_str(), F_OK) == 0);
    const std::vector<std::string> command = {black, "solve", "-f",
                                              ltl_formula};
    const ProcessResult result = execute_and_capture(command);
    // Check UNSAT before SAT: the former contains the latter as a substring.
    bool sat = false;
    if (result.m_output.find("UNSAT") != std::string::npos) {
        sat = false;
    } else if (result.m_output.find("SAT") != std::string::npos) {
        sat = true;
    } else {
        assert(false);
    }
    m_cache.emplace(ltl_formula, sat);
    return sat;
}
