#include "runner/ganak.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "prop_formula.hpp"

namespace {

struct ProcessResult {
    int m_exit_code;
    std::string m_output;
};

std::string temp_directory() {
    try {
        return std::filesystem::temp_directory_path().string();
    } catch (const std::filesystem::filesystem_error&) {
        // Keep a deterministic fallback when no system temp directory resolves.
        return "/tmp";
    }
}

std::string write_temporary_dimacs(const std::string& contents) {
    std::string writable_template =
        temp_directory() + "/counter-formula-XXXXXX";
    std::vector<char> buffer(writable_template.begin(),
                             writable_template.end());
    buffer.push_back('\0');
    const int file_descriptor = mkstemp(buffer.data());
    assert(file_descriptor >= 0);
    close(file_descriptor);
    const std::string dimacs_path(buffer.data());
    std::ofstream dimacs_file(dimacs_path);
    assert(dimacs_file.good());
    dimacs_file << contents;
    dimacs_file.close();
    assert(static_cast<bool>(dimacs_file));
    return dimacs_path;
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
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0 ||
            dup2(pipe_fds[1], STDERR_FILENO) < 0) {
            _exit(127);
        }
        close(pipe_fds[1]);
        std::unique_ptr<char*[]> argv =
            std::make_unique<char*[]>(arguments.size() + 1);
        for (std::size_t index = 0; index < arguments.size(); ++index) {
            argv[index] = const_cast<char*>(arguments[index].c_str());
        }
        argv[arguments.size()] = nullptr;
        execv(arguments[0].c_str(), argv.get());
        _exit(127);
    }
    close(pipe_fds[1]);
    std::string output;
    char read_buffer[4096];
    while (true) {
        const ssize_t bytes_read =
            read(pipe_fds[0], read_buffer, sizeof(read_buffer));
        if (bytes_read > 0) {
            output.append(read_buffer, static_cast<std::size_t>(bytes_read));
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

Count parse_ganak_exact_count(const std::string& output) {
    const std::string prefix = "c s exact arb int ";
    const std::size_t prefix_position = output.find(prefix);
    if (prefix_position != std::string::npos) {
        const std::size_t number_start = prefix_position + prefix.size();
        std::size_t number_end = number_start;
        while (number_end < output.size() &&
               std::isdigit(static_cast<unsigned char>(output[number_end]))) {
            ++number_end;
        }
        if (number_end > number_start) {
            const std::string number_text =
                output.substr(number_start, number_end - number_start);
            return parse_count_decimal_or_throw(number_text);
        }
    }
    if (output.find("s UNSATISFIABLE") != std::string::npos) {
        return 0;
    }
    assert(false);
    return 0;
}

}  // namespace

std::string ganak_executable_path() {
#ifdef GANAK_EXECUTABLE_PATH
    return GANAK_EXECUTABLE_PATH;
#else
    assert(false);
    return "";
#endif
}

Count run_ganak_on_dimacs(const std::string& dimacs_path, unsigned seed) {
    assert(access(dimacs_path.c_str(), F_OK) == 0);
    const std::string ganak_path = ganak_executable_path();
    assert(access(ganak_path.c_str(), F_OK) == 0);
    const std::vector<std::string> command = {
        ganak_path,
        "--seed",
        std::to_string(seed),
        dimacs_path,
    };
    const ProcessResult result = execute_and_capture(command);
    assert(result.m_exit_code == 0);
    return parse_ganak_exact_count(result.m_output);
}

Count run_ganak_on_formula(const std::string& formula, unsigned seed) {
    const Formula f = Formula(formula);
    const std::string formula_dimacs_path =
        write_temporary_dimacs(f.to_dimacs());
    const Count count = run_ganak_on_dimacs(formula_dimacs_path, seed);
    std::remove(formula_dimacs_path.c_str());
    return count;
}
