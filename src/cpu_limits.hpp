#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

// The parsing half of available_parallelism()'s cgroup bound, split out so it
// can be tested: the reads themselves are of fixed paths that only exist under
// a container, so nothing else here is reachable from a unit test.
//
// Both functions return nullopt for "no bound", which covers an explicitly
// unlimited quota and a file this build does not understand alike -- a bound
// that cannot be read must not tighten the worker count.

// Parses the contents of cgroup v2's cpu.max, which is "$MAX $PERIOD" where
// $MAX is either a positive integer or the literal "max" for unlimited.
std::optional<std::size_t> parse_cgroup_v2_cpu_max(std::string_view content);

// Parses the contents of cgroup v1's cpu.cfs_quota_us against its
// cpu.cfs_period_us. A quota of -1 means unlimited.
std::optional<std::size_t> parse_cgroup_v1_cpu_quota(std::string_view quota,
                                                     std::string_view period);
