#include "runner/simplify_batcher.hpp"

#include <fcntl.h>
#include <spawn.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "profile.hpp"
#include "runner/subprocess.hpp"

namespace {

// run_ltlfilt_batch drives a bidirectional pipe pair, which run_subprocess does
// not cover (it only captures output), so it spawns and reaps for itself and
// needs this.
double rusage_cpu_seconds(const struct rusage& usage) {
    const double user_s = static_cast<double>(usage.ru_utime.tv_sec) +
                          (static_cast<double>(usage.ru_utime.tv_usec) / 1e6);
    const double sys_s = static_cast<double>(usage.ru_stime.tv_sec) +
                         (static_cast<double>(usage.ru_stime.tv_usec) / 1e6);
    return user_s + sys_s;
}

// Runs one ltlfilt over @p formulas, writing each on its own line and reading
// one answer per line back. Returns false if anything about the exchange was
// not exactly one line out per line in, in which case the caller falls back to
// a formula-at-a-time exec rather than risk mismatching answers to formulae.
//
// Batch, not a persistent stream: ltlfilt does not answer line by line. It
// flushes in irregular lumps and holds the remainder until stdin reaches EOF
// (see PROFILING.md), so a request/response protocol over a long-lived child
// deadlocks. Closing stdin after the whole batch is what makes it emit
// everything.
bool run_ltlfilt_batch(const std::string& binary,
                       const std::vector<std::string>& formulas,
                       std::vector<std::string>& out, double& child_cpu_s,
                       std::chrono::milliseconds timeout) {
    COUNTER_PROFILE_SCOPE("ltlfilt/batch-exec");
    assert(!formulas.empty());
    std::string input;
    for (const std::string& formula : formulas) {
        input += formula;
        input += '\n';
    }

    std::array<int, 2> in_pipe = {-1, -1};
    std::array<int, 2> out_pipe = {-1, -1};
    if (pipe2(in_pipe.data(), O_CLOEXEC) != 0) {
        return false;
    }
    if (pipe2(out_pipe.data(), O_CLOEXEC) != 0) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        return false;
    }

    const std::array<std::string, 5> args = {binary, "--simplify",
                                             "--skip-errors", "-F", "-"};
    std::array<char*, 6> argv{};
    for (std::size_t i = 0; i < args.size(); ++i) {
        argv[i] = const_cast<char*>(args[i].c_str());
    }
    argv[args.size()] = nullptr;

    pid_t child_pid = -1;
    {
        COUNTER_PROFILE_SCOPE("proc/fork+exec");
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, in_pipe[0], STDIN_FILENO);
        posix_spawn_file_actions_adddup2(&actions, out_pipe[1], STDOUT_FILENO);
        const int spawn_result =
            posix_spawn(&child_pid, binary.c_str(), &actions, nullptr,
                        argv.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
        if (spawn_result != 0) {
            close(in_pipe[0]);
            close(in_pipe[1]);
            close(out_pipe[0]);
            close(out_pipe[1]);
            return false;
        }
    }
    close(in_pipe[0]);
    close(out_pipe[1]);

    // The batch is capped (see k_max_batch_bytes) so the whole input fits in
    // the pipe buffer. Without that cap this write could block while the child
    // blocks writing answers into an output pipe nobody is draining yet.
    bool wrote = true;
    std::size_t written = 0;
    while (written < input.size()) {
        const ssize_t bytes_written =
            write(in_pipe[1], input.data() + written, input.size() - written);
        if (bytes_written > 0) {
            written += static_cast<std::size_t>(bytes_written);
            continue;
        }
        if (bytes_written < 0 && errno == EINTR) {
            continue;
        }
        wrote = false;
        break;
    }
    close(in_pipe[1]);

    // The analyser reaches here from SimplifyBatcher::simplify, which holds a
    // lock at its call site -- but it releases that lock before calling this,
    // precisely so the exec runs outside it. Blocking here is the leader
    // waiting on its own child, not on a mutex.
    auto [output, timed_out] =
        read_until_eof(  // NOLINT(clang-analyzer-unix.BlockInCriticalSection)
            out_pipe[0], timeout);
    if (timed_out) {
        // Killed before the wait below, which would otherwise block on exactly
        // the child that has run out of time. Every formula in the batch is
        // then reported as a failure and retried one at a time, each under its
        // own copy of the same deadline, so the one that overran is the only
        // one that ends up unsimplified.
        kill(child_pid, SIGKILL);
    }
    close(out_pipe[0]);

    int wait_status = 0;
    struct rusage child_usage{};
    {
        COUNTER_PROFILE_SCOPE("proc/wait");
        [[maybe_unused]] const pid_t waited =
            wait4(child_pid, &wait_status, 0, &child_usage);
    }
    // Reported back rather than accumulated here. LtlfiltStats has no internal
    // synchronisation and simplify_ltl already updates it under its cache lock;
    // adding to it from this thread under any other lock is a data race on the
    // same variable, which is exactly what ThreadSanitizer flagged. The leader
    // books the whole batch's child CPU against its own call.
    child_cpu_s = rusage_cpu_seconds(child_usage);
    if (!wrote || timed_out) {
        return false;
    }

    out.clear();
    out.reserve(formulas.size());
    std::size_t start = 0;
    while (start < output.size()) {
        const std::size_t end = output.find('\n', start);
        if (end == std::string::npos) {
            break;
        }
        out.emplace_back(output, start, end - start);
        start = end + 1;
    }
    // The count check is the safety property: one short or extra line means
    // answers no longer line up with formulae, and a silently misattributed
    // simplification would corrupt the search rather than slow it down.
    return out.size() == formulas.size();
}

// Bounded so a batch's input always fits in a pipe buffer; see the write loop
// in run_ltlfilt_batch. Whatever does not fit stays queued for the next leader.
// Also bounds a single formula, through is_batchable: take_batch admits its
// first formula whatever its size, since a batch of nothing makes no progress,
// so the cap alone would not keep an oversized one out of a batch.
constexpr std::size_t k_max_batch_bytes = 16384;

// Three things disqualify a formula. One spanning lines cannot go in a
// line-oriented batch; a blank one is consumed by ltlfilt without producing
// an answer, which would fail the batch's line-count check; and one larger
// than the cap would break the assumption that a whole batch fits in a pipe
// buffer, deadlocking the leader against its own child. The blank case is
// the specification formula of a candidate with no guarantees, and the
// oversized case is what the search builds, so neither is hypothetical.
bool is_batchable(const std::string& formula) {
    return formula.find('\n') == std::string::npos &&
           formula.find_first_not_of(" \t\r") != std::string::npos &&
           formula.size() + 1 <= k_max_batch_bytes;
}

// Coalesces concurrent simplify_ltl misses into one ltlfilt exec.
//
// No timer and no artificial delay: whichever caller finds no leader takes
// everything queued right now and runs it. While that exec is in flight the
// other scoring threads pile up behind it, so the next batch is naturally as
// large as the contention warrants and collapses to a batch of one when only
// one thread is asking. Startup is ~9ms and ~2700 minor faults per exec
// regardless of batch size, so this is the whole saving.
class SimplifyBatcher {
   public:
    // std::nullopt means "run it yourself": the batch failed its line-count
    // check, or ltlfilt could not be spawned.
    // child_cpu_s receives the batch's child CPU when this caller ran the
    // batch, and is left alone when another caller did, so the total is counted
    // once.
    std::optional<std::string> simplify(const std::string& binary,
                                        const std::string& formula,
                                        double& child_cpu_s,
                                        std::chrono::milliseconds timeout) {
        auto slot = std::make_shared<Slot>();
        slot->m_formula = formula;
        std::unique_lock<std::mutex> lock(m_mutex);
        m_pending.push_back(slot);
        while (!slot->m_ready) {
            if (m_leader) {
                m_done.wait(lock);
                continue;
            }
            m_leader = true;
            std::vector<std::shared_ptr<Slot>> batch = take_batch();
            lock.unlock();
            std::vector<std::string> results;
            bool batch_ok = false;
            // Nothing here is expected to throw, but an exception escaping
            // would leave m_leader set and every slot un-ready, which parks all
            // the other callers on a condition variable nobody will ever signal
            // again. A hang is a far worse failure than the exception itself,
            // so resolve the batch first and rethrow after.
            try {
                std::vector<std::string> inputs;
                inputs.reserve(batch.size());
                for (const auto& queued : batch) {
                    inputs.push_back(queued->m_formula);
                }
                // The configured budget is per formula, so a batch of n gets
                // n times it: charging a whole batch of cheap formulae one
                // formula's worth would kill work making normal progress.
                // That makes the batch's own bound looser than the key asks
                // for, and the fallback tightens it back up -- a batch that
                // overruns fails, and each of its formulae is then retried
                // alone under exactly one budget, so only the formula that
                // actually overran goes unsimplified.
                batch_ok = run_ltlfilt_batch(
                    binary, inputs, results, child_cpu_s,
                    timeout * static_cast<std::int64_t>(batch.size()));
            } catch (...) {
                lock.lock();
                retire_batch(batch, false, results);
                m_done.notify_all();
                throw;
            }
            lock.lock();
            retire_batch(batch, batch_ok, results);
            m_done.notify_all();
        }
        if (slot->m_failed) {
            return std::nullopt;
        }
        return slot->m_result;
    }

   private:
    struct Slot {
        std::string m_formula;
        std::string m_result;
        bool m_ready = false;
        bool m_failed = false;
    };

    static constexpr std::size_t k_max_batch_count = 64;

    // Marks every slot in the batch resolved and steps the leader down. The
    // caller must hold m_mutex and must notify afterwards. A slot left un-ready
    // would park its caller forever, so this runs on the failure paths too.
    void retire_batch(const std::vector<std::shared_ptr<Slot>>& batch,
                      bool batch_ok, const std::vector<std::string>& results) {
        const bool usable = batch_ok && results.size() == batch.size();
        for (std::size_t i = 0; i < batch.size(); ++i) {
            batch[i]->m_failed = !usable;
            if (usable) {
                batch[i]->m_result = results[i];
            }
            batch[i]->m_ready = true;
        }
        m_leader = false;
    }

    std::vector<std::shared_ptr<Slot>> take_batch() {
        std::vector<std::shared_ptr<Slot>> batch;
        std::size_t bytes = 0;
        while (!m_pending.empty() && batch.size() < k_max_batch_count) {
            const std::size_t size = m_pending.front()->m_formula.size() + 1;
            if (!batch.empty() && bytes + size > k_max_batch_bytes) {
                break;
            }
            bytes += size;
            batch.push_back(m_pending.front());
            m_pending.erase(m_pending.begin());
        }
        return batch;
    }

    std::mutex m_mutex;
    std::condition_variable m_done;
    std::vector<std::shared_ptr<Slot>> m_pending;
    bool m_leader = false;
};

// Several batchers rather than one: a single leader would serialise every
// simplification in the process behind one exec at a time. Each batcher runs
// its own exec concurrently, so throughput stays parallel while each exec still
// amortises its startup over a whole batch.
// How many is a latency-versus-CPU trade with no free setting, so it is
// configurable rather than fixed; see Config::ltlfilt_batchers.
std::atomic<std::size_t> g_batcher_count{4};

// Built once, on the first simplification, and never resized: scoring threads
// index into it without a lock, so growing it underneath them would be a race.
// set_ltlfilt_batchers therefore has to run before any scoring starts.
std::vector<std::unique_ptr<SimplifyBatcher>>& batchers() {
    static std::vector<std::unique_ptr<SimplifyBatcher>> pool = [] {
        std::vector<std::unique_ptr<SimplifyBatcher>> built;
        const std::size_t count = g_batcher_count.load();
        built.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            built.push_back(std::make_unique<SimplifyBatcher>());
        }
        return built;
    }();
    return pool;
}

// count must be positive: it is a modulus. Callers check the pool is non-empty
// first, since a zero pool means batching is switched off rather than that
// slot 0 should be used.
std::size_t batcher_slot(std::size_t count) {
    assert(count > 0);
    static std::atomic<std::size_t> next{0};
    thread_local const std::size_t slot =
        next.fetch_add(1, std::memory_order_relaxed);
    return slot % count;
}

}  // namespace

void set_ltlfilt_batchers(std::size_t count) { g_batcher_count.store(count); }

std::optional<std::string> batched_simplify(const std::string& binary,
                                            const std::string& formula,
                                            double& child_cpu_s,
                                            std::chrono::milliseconds timeout) {
    if (!is_batchable(formula)) {
        return std::nullopt;
    }
    // An empty pool is ltlfilt_batchers = 0, which turns batching off: every
    // call gets its own exec, as it did before batching existed. The emptiness
    // check has to come first -- batcher_slot takes the pool size as a modulus,
    // so reaching it with an empty pool is a division by zero.
    const std::size_t pool_size = batchers().size();
    if (pool_size == 0) {
        return std::nullopt;
    }
    // Below both early returns, so the scope counts requests that were actually
    // batched. Above them it would report a call per formula with batching
    // switched off, which is precisely the configuration ltlfilt_batchers = 0
    // exists to measure against.
    COUNTER_PROFILE_SCOPE("ltlfilt/batched-request");
    return batchers()[batcher_slot(pool_size)]->simplify(binary, formula,
                                                         child_cpu_s, timeout);
}
