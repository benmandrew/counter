#include "runner/spot.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "requirement.hpp"

namespace {

struct ProcessResult {
    int m_exit_code;
    std::string m_output;
};

void spawn_child_and_exec(const std::vector<std::string>& arguments,
                          int write_fd) {
    if (dup2(write_fd, STDOUT_FILENO) < 0 ||
        dup2(write_fd, STDERR_FILENO) < 0) {
        _exit(127);
    }
    close(write_fd);
    std::unique_ptr<char*[]> argv =
        std::make_unique<char*[]>(arguments.size() + 1);
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        argv[i] = const_cast<char*>(arguments[i].c_str());
    }
    argv[arguments.size()] = nullptr;
    execv(arguments[0].c_str(), argv.get());
    _exit(127);
}

std::string read_from_fd(int read_fd) {
    std::string output;
    char buffer[4096];
    while (true) {
        const ssize_t bytes_read = read(read_fd, buffer, sizeof(buffer));
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
        throw std::runtime_error(std::string("read() failed: ") +
                                 std::strerror(errno));
    }
    return output;
}

int wait_for_child(pid_t child_pid) {
    int wait_status = 0;
    if (waitpid(child_pid, &wait_status, 0) < 0) {
        throw std::runtime_error(std::string("waitpid() failed: ") +
                                 std::strerror(errno));
    }
    if (WIFEXITED(wait_status)) {
        return WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        return 128 + WTERMSIG(wait_status);
    }
    return -1;
}

ProcessResult execute_and_capture(const std::vector<std::string>& arguments) {
    if (arguments.empty()) {
        throw std::invalid_argument(
            "No command provided to execute_and_capture.");
    }
    int pipe_fds[2] = {-1, -1};
    if (pipe(pipe_fds) != 0) {
        throw std::runtime_error(std::string("pipe() failed: ") +
                                 std::strerror(errno));
    }
    const pid_t child_pid = fork();
    if (child_pid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        throw std::runtime_error(std::string("fork() failed: ") +
                                 std::strerror(errno));
    }
    if (child_pid == 0) {
        close(pipe_fds[0]);
        spawn_child_and_exec(arguments, pipe_fds[1]);
    }
    close(pipe_fds[1]);
    std::string output = read_from_fd(pipe_fds[0]);
    close(pipe_fds[0]);
    int exit_code = wait_for_child(child_pid);
    return {exit_code, output};
}

std::string join_comma(const std::vector<std::string>& items) {
    std::string result;
    bool first = true;
    for (const auto& item : items) {
        if (!first) result += ',';
        result += item;
        first = false;
    }
    return result;
}

}  // namespace

std::string spot_bin_dir() {
#ifdef SPOT_BIN_DIR
    return SPOT_BIN_DIR;
#else
    throw std::runtime_error("SPOT_BIN_DIR is not configured by CMake.");
#endif
}

std::string ltlsynt_path() { return spot_bin_dir() + "/ltlsynt"; }

bool check_realizability(const Requirement& requirement) {
    if (!requirement.m_ltl.has_value()) {
        throw std::invalid_argument(
            "Requirement must have m_ltl set to check realizability.");
    }
    const std::string ltlsynt = ltlsynt_path();
    if (access(ltlsynt.c_str(), F_OK) != 0) {
        throw std::runtime_error("ltlsynt executable does not exist: " +
                                 ltlsynt);
    }
    const LtlSpec& spec = *requirement.m_ltl;
    std::vector<std::string> command = {ltlsynt, "--realizability", "-f",
                                        spec.m_ltl};
    // Specify only one side and let ltlsynt infer the other. Specifying both
    // triggers a strict validation in ltlsynt that rejects atoms it considers
    // not to match the formula.
    if (!spec.m_in_atoms.empty()) {
        command.push_back("--ins=" + join_comma(spec.m_in_atoms));
    } else if (!spec.m_out_atoms.empty()) {
        command.push_back("--outs=" + join_comma(spec.m_out_atoms));
    }
    const ProcessResult result = execute_and_capture(command);
    // Check UNREALIZABLE before REALIZABLE: the former contains the latter as a
    // substring.
    if (result.m_output.find("UNREALIZABLE") != std::string::npos) {
        return false;
    }
    if (result.m_output.find("REALIZABLE") != std::string::npos) {
        return true;
    }
    throw std::runtime_error("ltlsynt produced unexpected output (exit code " +
                             std::to_string(result.m_exit_code) +
                             "): " + result.m_output);
}
