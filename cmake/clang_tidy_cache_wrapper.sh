#!/usr/bin/env bash
# clang-tidy interposer used only when the lint path opts into ctcache.
#
# run-clang-tidy calls its -clang-tidy-binary as `<binary> <clang-tidy-args...>`
# per translation unit. clang-tidy-cache instead expects the real clang-tidy as
# its first argument (`clang-tidy-cache <real-clang-tidy> <args...>`), so this
# script bridges the two: it prepends the cache launcher and the real binary,
# both supplied by cmake/run_clang_tidy.cmake via the environment. clang-tidy's
# exit code (including WarningsAsErrors) is propagated unchanged via exec.
#
# This wrapper is referenced only when CTCACHE_DIR is set and a clang-tidy-cache
# binary is on PATH; otherwise run-clang-tidy invokes the real clang-tidy
# directly and this file is never touched.
set -euo pipefail
exec "${CTCACHE_CACHE_BIN:?CTCACHE_CACHE_BIN not set}" \
     "${CTCACHE_REAL_CLANG_TIDY:?CTCACHE_REAL_CLANG_TIDY not set}" \
     "$@"
