#include "runner/ltlfilt.hpp"

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
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.hpp"
#include "profile.hpp"
#include "runner/spot.hpp"
#include "runner/spot_inprocess.hpp"
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
                       std::vector<std::string>& out, double& child_cpu_s) {
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

    std::string output;
    {
        COUNTER_PROFILE_SCOPE("proc/read");
        std::array<char, 4096> buf{};
        while (true) {
            // The analyser reaches here from SimplifyBatcher::simplify, which
            // holds a lock at its call site -- but it releases that lock before
            // calling this, precisely so the exec runs outside it. Blocking
            // here is the leader waiting on its own child, not on a mutex.
            const ssize_t bytes_read =
                read(  // NOLINT(clang-analyzer-unix.BlockInCriticalSection)
                    out_pipe[0], buf.data(), buf.size());
            if (bytes_read > 0) {
                output.append(buf.data(), static_cast<std::size_t>(bytes_read));
                continue;
            }
            if (bytes_read < 0 && errno == EINTR) {
                continue;
            }
            break;
        }
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
    if (!wrote) {
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
                                        double& child_cpu_s) {
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
                batch_ok =
                    run_ltlfilt_batch(binary, inputs, results, child_cpu_s);
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

    // Bounded so the batch's input always fits in a pipe buffer; see the write
    // loop in run_ltlfilt_batch. Whatever does not fit stays queued for the
    // next leader.
    static constexpr std::size_t k_max_batch_bytes = 16384;
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

std::atomic<SimplifyEngine> g_simplify_engine{SimplifyEngine::Libspot};

// How long a caller will wait for the libspot lock before spawning ltlfilt
// instead. Set to roughly what a spawn costs (measured at 8-9 ms, dominated by
// the child demand-paging its own executable), because that is exactly the
// point where waiting stops being the cheaper of the two.
constexpr std::chrono::milliseconds k_libspot_lock_budget{8};

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

std::string ltlfilt_path() { return spot_bin_dir() + "/ltlfilt"; }

void set_ltlfilt_batchers(std::size_t count) { g_batcher_count.store(count); }

void set_simplify_engine(SimplifyEngine engine) {
    g_simplify_engine.store(engine);
}

std::string simplify_ltl(const std::string& formula) {
    COUNTER_PROFILE_SCOPE("ltlfilt/simplify_ltl");
    static std::unordered_map<std::string, std::string> cache;
    static std::mutex cache_mutex;
    {
        COUNTER_PROFILE_SCOPE("ltlfilt/simplify_ltl:cache-lookup");
        std::scoped_lock lock(cache_mutex);
        const auto found = cache.find(formula);
        if (found != cache.end()) {
            LtlfiltStats::n_cache_hits++;
            return found->second;
        }
        LtlfiltStats::n_cache_misses++;
    }
    const auto start = std::chrono::steady_clock::now();
    // The in-process engine needs no binary on disk and spawns nothing, so it
    // is tried before the ltlfilt path is even looked for. A formula it cannot
    // parse falls through to the exec, which is what used to happen to a
    // formula ltlfilt could not parse: the formula is returned unchanged.
    if (g_simplify_engine.load(std::memory_order_relaxed) ==
        SimplifyEngine::Libspot) {
        SpotSimplification in_process =
            spot_try_simplify(formula, k_libspot_lock_budget);
        // Falling back needs something to fall back to. Without the binary the
        // exec path below returns the formula unsimplified, which would make a
        // busy lock silently change the answer, so wait for the lock instead.
        if (in_process.m_lock_busy &&
            access(ltlfilt_path().c_str(), F_OK) != 0) {
            in_process.m_formula = spot_simplify(formula);
            in_process.m_lock_busy = false;
        }
        // Busy means another thread is inside libspot and this one would wait
        // longer than a spawn costs, so it spawns instead. Everything below is
        // the exec path, unchanged.
        if (!in_process.m_lock_busy) {
            const double elapsed = std::chrono::duration<double>(
                                       std::chrono::steady_clock::now() - start)
                                       .count();
            std::scoped_lock lock(cache_mutex);
            LtlfiltStats::total_time_s += elapsed;
            const std::string& simplified =
                in_process.m_formula.value_or(formula);
            cache.emplace(formula, simplified);
            return simplified;
        }
    }
    const std::string binary = ltlfilt_path();
    if (access(binary.c_str(), F_OK) != 0) {
        std::scoped_lock lock(cache_mutex);
        cache.emplace(formula, formula);
        return formula;
    }
    std::string simplified = formula;
    double child_cpu_s = 0.0;
    // A formula spanning lines cannot go in a line-oriented batch, and a blank
    // one is consumed by ltlfilt without producing an answer, which would make
    // the batch fail its line-count check. Both go straight to a private exec.
    // The blank case is reachable: it is the specification formula of a
    // candidate with no guarantees.
    const bool batchable =
        formula.find('\n') == std::string::npos &&
        formula.find_first_not_of(" \t\r") != std::string::npos;
    std::optional<std::string> batched;
    // An empty pool is ltlfilt_batchers = 0, which turns batching off: every
    // call gets its own exec, as it did before batching existed. The emptiness
    // check has to come first -- batcher_slot takes the pool size as a modulus,
    // so reaching it with an empty pool is a division by zero.
    const std::size_t pool_size = batchers().size();
    if (batchable && pool_size > 0) {
        COUNTER_PROFILE_SCOPE("ltlfilt/batched-request");
        batched = batchers()[batcher_slot(pool_size)]->simplify(binary, formula,
                                                                child_cpu_s);
    }
    if (batched.has_value()) {
        simplified = *batched;
    } else {
        COUNTER_PROFILE_SCOPE("ltlfilt/one-shot-exec");
        const SubprocessResult result =
            run_subprocess({binary, "--simplify", "-f", formula});
        child_cpu_s = result.m_cpu_s;
        if (result.m_exit_code == 0 && !result.m_output.empty()) {
            simplified = result.m_output;
            while (!simplified.empty() && simplified.back() == '\n') {
                simplified.pop_back();
            }
        }
    }
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    std::scoped_lock lock(cache_mutex);
    LtlfiltStats::total_time_s += elapsed;
    LtlfiltStats::total_cpu_s += child_cpu_s;
    cache.emplace(formula, simplified);
    return simplified;
}

std::string normalize_ltl(const std::string& formula) {
    std::string simplified = simplify_ltl(formula);
    // SPOT uses "0"/"1" for the boolean constants false/true. There is no
    // single keyword accepted by all downstream tools (black treats "false" as
    // an atom, not a constant), so fall back to the original formula in these
    // cases to preserve correctness.
    if (simplified == "0" || simplified == "1") {
        return formula;
    }
    return simplified;
}

bool ltl_equivalent(const std::string& lhs, const std::string& rhs) {
    const std::string binary = ltlfilt_path();
    if (access(binary.c_str(), F_OK) != 0) {
        return true;
    }
    const SubprocessResult result =
        run_subprocess({binary, "--equivalent-to=" + rhs, "-f", lhs});
    // ltlfilt's filter convention: exit 0 means the input formula (lhs)
    // matched (i.e. is equivalent to rhs); exit 1 means it didn't. Any other
    // status (parse error, crash) is inconclusive, not a mismatch.
    if (result.m_exit_code == 0) {
        return true;
    }
    if (result.m_exit_code == 1) {
        return false;
    }
    return true;
}
