#include "runner/spot.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
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
        assert(false);
    }
    return output;
}

int wait_for_child(pid_t child_pid) {
    int wait_status = 0;
    assert(waitpid(child_pid, &wait_status, 0) >= 0);
    if (WIFEXITED(wait_status)) {
        return WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        return 128 + WTERMSIG(wait_status);
    }
    return -1;
}

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
    assert(!specification.m_requirements.empty());
    for (const Requirement& req : specification.m_requirements) {
        assert(req.m_ltl.has_value());
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
        assert(false);
        return false;
    }
}

}  // namespace

std::string spot_bin_dir() {
#ifdef SPOT_BIN_DIR
    return SPOT_BIN_DIR;
#else
    assert(false);
    return "";
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
    assert(access(ltlsynt.c_str(), F_OK) == 0);
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
