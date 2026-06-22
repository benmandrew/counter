#pragma once

/// @file crash_handler.hpp
/// @brief Signal handlers (SIGSEGV/SIGABRT/SIGFPE) that fork a symbol-resolver
///        child and write crash reports to crashes/.

#include <string>

/// Initialises cpptrace crash logging. Registers signal handlers for
/// SIGSEGV, SIGABRT, and SIGFPE that capture a raw stacktrace signal-safely
/// and fork+exec a `signal_tracer` child to resolve symbols and append a
/// crash report to `crashes/<pid>_<timestamp>.log`. Also registers
/// `cpptrace::register_terminate_handler()` for uncaught exceptions.
/// \a executable_name should be `argv[0]` so the handler can locate the
/// `signal_tracer` binary in the same directory as the executable.
void init_cpptrace(const char* executable_name);

/// Stores \a text to be written at the top of any crash report produced by the
/// signal handler. Safe to call any time after init_cpptrace(). Calling it
/// more than once overwrites the previous value.
void register_crash_metadata(const std::string& text);
