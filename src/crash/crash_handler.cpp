#include "crash/crash_handler.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <cpptrace/cpptrace.hpp>

namespace {

constexpr std::size_t k_path_buffer_size = 4096;
constexpr std::size_t k_num_buffer_size = 32;
constexpr std::size_t k_frame_buffer_size = 100;
constexpr std::size_t k_metadata_buffer_size = 4096;

std::array<char, k_path_buffer_size> g_tracer_path = {};
std::array<char, k_path_buffer_size> g_crash_dir = {};
std::array<char, k_metadata_buffer_size> g_crash_metadata = {};

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
              signal_buffer.data(), pid_buffer.data(), g_crash_metadata.data(),
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

}  // namespace

void register_crash_metadata(const std::string& text) {
    if (text.size() + 1 > g_crash_metadata.size()) {
        throw std::runtime_error("crash metadata exceeds buffer size");
    }
    std::memcpy(g_crash_metadata.data(), text.c_str(), text.size() + 1);
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
