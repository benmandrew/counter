#include "runner/spot.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
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

void check_specification_ltls_present(const Specification& specification) {
    if (specification.m_requirements.empty()) {
        throw std::invalid_argument("Specification must not be empty.");
    }
    for (const Requirement& req : specification.m_requirements) {
        if (!req.m_ltl.has_value()) {
            throw std::invalid_argument(
                "All requirements in specification must have m_ltl set to "
                "check realizability.");
        }
    }
}

void build_specification_conjunction(const Specification& specification,
                                     std::string& conj_ltl) {
    bool first = true;
    for (const Requirement& req : specification.m_requirements) {
        if (!first) {
            conj_ltl += " & ";
        }
        conj_ltl += "(" + req.m_ltl.value() + ")";
        first = false;
    }
}

bool parse_realizability_output(const ProcessResult& result) {
    if (result.m_output.find("UNREALIZABLE") != std::string::npos) {
        return false;
    } else if (result.m_output.find("REALIZABLE") != std::string::npos) {
        return true;
    } else {
        throw std::runtime_error(
            "ltlsynt produced unexpected output (exit code " +
            std::to_string(result.m_exit_code) + "): " + result.m_output);
    }
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

bool RealizabilityChecker::check_realizability(
    const Specification& specification) {
    check_specification_ltls_present(specification);
    std::string conj_ltl;
    build_specification_conjunction(specification, conj_ltl);
    const std::string cache_key = conj_ltl + "|" +
                                  join_comma(specification.m_in_atoms) + "|" +
                                  join_comma(specification.m_out_atoms);
    const auto it = m_cache.find(cache_key);
    if (it != m_cache.end()) {
        return it->second;
    }
    const std::string ltlsynt = ltlsynt_path();
    if (access(ltlsynt.c_str(), F_OK) != 0) {
        throw std::runtime_error("ltlsynt executable does not exist: " +
                                 ltlsynt);
    }
    std::vector<std::string> command = {ltlsynt, "--realizability", "-f",
                                        conj_ltl};
    if (!specification.m_in_atoms.empty()) {
        command.push_back("--ins=" + join_comma(specification.m_in_atoms));
    } else if (!specification.m_out_atoms.empty()) {
        command.push_back("--outs=" + join_comma(specification.m_out_atoms));
    }
    std::cout << "Executing command: ";
    for (const auto& arg : command) {
        std::cout << arg << " ";
    }
    std::cout << std::endl;
    const ProcessResult result = execute_and_capture(command);
    bool realizable = parse_realizability_output(result);
    m_cache.emplace(cache_key, realizable);
    return realizable;
}
