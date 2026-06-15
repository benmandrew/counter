#include <chrono>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <cpptrace/cpptrace.hpp>

namespace {

const char* signal_name(int signal_number) {
    switch (signal_number) {
        case SIGABRT:
            return "SIGABRT";
        case SIGSEGV:
            return "SIGSEGV";
        case SIGFPE:
            return "SIGFPE";
        default:
            return "UNKNOWN";
    }
}

std::string current_time_string() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time = {};
#if defined(_WIN32)
    localtime_s(&local_time, &now_time);
#else
    localtime_r(&now_time, &local_time);
#endif
    std::ostringstream stream;
    stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

}  // namespace

int main(int argc, char* argv[]) {
    cpptrace::object_trace trace;
    cpptrace::safe_object_frame frame{};
    while (std::fread(&frame, sizeof(frame), 1, stdin) == 1) {
        trace.frames.push_back(frame.resolve());
    }
    std::ostream* output = &std::cerr;
    std::ofstream file;
    if (argc >= 2 && argv[1] != nullptr) {
        file.open(argv[1], std::ios::app);
        if (file.is_open()) {
            output = &file;
        }
    }
    const int signal_number =
        argc >= 3 && argv[2] != nullptr ? std::atoi(argv[2]) : 0;
    const int pid = argc >= 4 && argv[3] != nullptr ? std::atoi(argv[3]) : 0;
    const char* metadata = argc >= 5 && argv[4] != nullptr && argv[4][0] != '\0'
                               ? argv[4]
                               : nullptr;
    *output << "=== CRASH REPORT ===\n";
    *output << "Time:   " << current_time_string() << "\n";
    *output << "Signal: " << signal_name(signal_number) << " (" << signal_number
            << ")\n";
    *output << "PID:    " << pid << "\n";
    if (metadata != nullptr) {
        *output << "\n" << metadata << "\n";
    }
    *output << "\nStack trace:\n";
    trace.resolve().print(*output, false);
    *output << '\n';
    return 0;
}
