#include "profile.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace profile {

namespace {

// Sites are heap-allocated and deliberately leaked. A worker thread can still
// be inside a Scope destructor while main() returns, and a Site destroyed under
// it would be a use-after-free in the one build (release, long campaign run)
// where the profiler is least welcome to crash.
std::vector<Site*>& site_registry() {
    static std::vector<Site*> registry;
    return registry;
}

std::mutex& registry_mutex() {
    static std::mutex mutex;
    return mutex;
}

std::map<std::string, std::uint64_t>& counter_registry() {
    static std::map<std::string, std::uint64_t> counters;
    return counters;
}

// COUNTER_PROFILE's value, or nullptr when unset. Captured once: a run that
// re-read the environment per scope would let the answer change mid-run.
const char* profile_target() {
    static const char* target = std::getenv("COUNTER_PROFILE");
    return target;
}

std::string format_ns(std::uint64_t nanos) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3)
        << (static_cast<double>(nanos) / 1e9) << "s";
    return out.str();
}

}  // namespace

bool enabled() {
    static const bool is_enabled = profile_target() != nullptr;
    return is_enabled;
}

std::uint64_t wall_ns() {
    struct timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (static_cast<std::uint64_t>(now.tv_sec) * 1'000'000'000ULL) +
           static_cast<std::uint64_t>(now.tv_nsec);
}

std::uint64_t thread_cpu_ns() {
    struct timespec now{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
    return (static_cast<std::uint64_t>(now.tv_sec) * 1'000'000'000ULL) +
           static_cast<std::uint64_t>(now.tv_nsec);
}

Site& site(const char* name) {
    const std::scoped_lock lock(registry_mutex());
    for (Site* existing : site_registry()) {
        if (std::strcmp(existing->m_name, name) == 0) {
            return *existing;
        }
    }
    auto* fresh = new Site(name);
    site_registry().push_back(fresh);
    return *fresh;
}

Site& site_interned(const std::string& name) {
    // Interned in a deque, not a vector: Site holds a bare pointer into these
    // strings, and a vector would invalidate every one of them when it grew.
    static std::deque<std::string> interned;
    {
        const std::scoped_lock lock(registry_mutex());
        for (Site* existing : site_registry()) {
            if (name == existing->m_name) {
                return *existing;
            }
        }
        interned.push_back(name);
    }
    return site(interned.back().c_str());
}

void add_count(const char* name, std::uint64_t n) {
    if (!enabled()) {
        return;
    }
    const std::scoped_lock lock(registry_mutex());
    counter_registry()[name] += n;
}

const std::vector<Site*>& sites() { return site_registry(); }

std::vector<std::pair<std::string, std::uint64_t>> counts() {
    const std::scoped_lock lock(registry_mutex());
    return {counter_registry().begin(), counter_registry().end()};
}

void report(std::ostream& out) {
    std::vector<Site*> ordered;
    {
        const std::scoped_lock lock(registry_mutex());
        ordered = site_registry();
    }
    ordered.erase(std::remove_if(ordered.begin(), ordered.end(),
                                 [](const Site* entry) {
                                     return entry->m_calls.load() == 0;
                                 }),
                  ordered.end());
    std::sort(ordered.begin(), ordered.end(),
              [](const Site* lhs, const Site* rhs) {
                  return lhs->m_wall_ns.load() > rhs->m_wall_ns.load();
              });

    out << "\nScope profile (COUNTER_PROFILE):\n";
    out << std::left << std::setw(34) << "site" << std::right << std::setw(10)
        << "calls" << std::setw(12) << "wall" << std::setw(12) << "cpu"
        << std::setw(9) << "cpu/wall" << std::setw(12) << "wall/call"
        << std::setw(12) << "max call" << "\n";
    for (const Site* entry : ordered) {
        const std::uint64_t calls = entry->m_calls.load();
        const std::uint64_t wall = entry->m_wall_ns.load();
        const std::uint64_t cpu = entry->m_cpu_ns.load();
        std::ostringstream ratio;
        ratio << std::fixed << std::setprecision(2)
              << (wall == 0
                      ? 0.0
                      : static_cast<double>(cpu) / static_cast<double>(wall));
        std::ostringstream per_call;
        per_call << std::fixed << std::setprecision(3)
                 << (static_cast<double>(wall) / static_cast<double>(calls) /
                     1e6)
                 << "ms";
        out << std::left << std::setw(34) << entry->m_name << std::right
            << std::setw(10) << calls << std::setw(12) << format_ns(wall)
            << std::setw(12) << format_ns(cpu) << std::setw(9) << ratio.str()
            << std::setw(12) << per_call.str() << std::setw(12)
            << format_ns(entry->m_max_wall_ns.load()) << "\n";
    }

    const auto counters = counts();
    if (!counters.empty()) {
        out << "\nScope counters:\n";
        for (const auto& [name, value] : counters) {
            out << std::left << std::setw(34) << name << std::right
                << std::setw(14) << value << "\n";
        }
    }
    out << std::flush;
}

void report_json(const std::string& path) {
    std::ofstream file(path);
    if (!file) {
        std::cerr << "profile: cannot write " << path << "\n";
        return;
    }
    std::vector<Site*> ordered;
    {
        const std::scoped_lock lock(registry_mutex());
        ordered = site_registry();
    }
    file << R"({)"
            "\n"
            R"(  "sites": [)"
            "\n";
    bool first = true;
    for (const Site* entry : ordered) {
        if (entry->m_calls.load() == 0) {
            continue;
        }
        if (!first) {
            file << ",\n";
        }
        first = false;
        file << R"(    {"name": ")" << entry->m_name << R"(", "calls": )"
             << entry->m_calls.load() << R"(, "wall_ns": )"
             << entry->m_wall_ns.load() << R"(, "cpu_ns": )"
             << entry->m_cpu_ns.load() << R"(, "max_wall_ns": )"
             << entry->m_max_wall_ns.load() << "}";
    }
    file << "\n"
            R"(  ],)"
            "\n"
            R"(  "counters": {)"
            "\n";
    first = true;
    for (const auto& [name, value] : counts()) {
        if (!first) {
            file << ",\n";
        }
        first = false;
        file << R"(    ")" << name << R"(": )" << value;
    }
    file << "\n  }\n}\n";
}

void report_if_enabled() {
    if (!enabled()) {
        return;
    }
    static std::once_flag once;
    std::call_once(once, [] {
        report(std::cerr);
        const char* target = profile_target();
        // "1" and "-" ask for the stderr table only; anything else is taken as
        // a path for the machine-readable copy.
        if (target != nullptr && std::strcmp(target, "1") != 0 &&
            std::strcmp(target, "-") != 0) {
            report_json(target);
        }
    });
}

}  // namespace profile
