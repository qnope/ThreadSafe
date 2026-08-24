#include <threadsafe/threadsafe.h>

#include "perf_bench.h"

#include <atomic>
#include <barrier>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

struct payload {
    int counters[8];
};

static_assert(threadsafe::is_synchronizable_v<const payload>,
              "the library must pick std::shared_mutex for this payload");

using library_value = threadsafe::synchronized_value<payload>;
static_assert(std::is_same_v<library_value::mutex, std::shared_mutex>);

struct hand_rolled_mutex_value {
    std::mutex mutex;
    payload value{};

    template <class Body>
    void read(Body body) const {
        std::lock_guard<std::mutex> held{const_cast<std::mutex&>(mutex)};
        body(value);
    }
    template <class Body>
    void write(Body body) {
        std::lock_guard<std::mutex> held{mutex};
        body(value);
    }
};

struct hand_rolled_shared_mutex_value {
    mutable std::shared_mutex mutex;
    payload value{};

    template <class Body>
    void read(Body body) const {
        std::shared_lock<std::shared_mutex> held{mutex};
        body(value);
    }
    template <class Body>
    void write(Body body) {
        std::lock_guard<std::shared_mutex> held{mutex};
        body(value);
    }
};

std::atomic<std::uint64_t> global_sink{0};

constexpr std::uint64_t operations_per_thread = 100000;

// Reads sum the payload; writes bump it. Identical work in every variant.
inline std::uint64_t read_body(const payload& value) {
    std::uint64_t total = 0;
    for (int index = 0; index < 8; ++index)
        total += std::uint64_t(value.counters[index]);
    return total;
}
inline void write_body(payload& value) {
    for (int index = 0; index < 8; ++index)
        ++value.counters[index];
}

template <class Runner>
double run_threads(int thread_count, Runner runner) {
    std::barrier start_line{thread_count + 1};
    std::vector<std::jthread> workers;
    workers.reserve(thread_count);
    for (int thread_index = 0; thread_index < thread_count; ++thread_index)
        workers.emplace_back([&, thread_index] {
            start_line.arrive_and_wait();
            runner(thread_index);
        });
    const auto start = std::chrono::steady_clock::now();
    start_line.arrive_and_wait();
    workers.clear();
    const auto stop = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(stop - start).count()
         / double(std::uint64_t(thread_count) * operations_per_thread);
}

template <class Value>
double bench_variant(Value& guarded, int thread_count, int writes_per_thousand) {
    return run_threads(thread_count, [&](int thread_index) {
        std::uint32_t random_state = 0x9e3779b9u + std::uint32_t(thread_index) * 2654435761u;
        std::uint64_t local_sink = 0;
        for (std::uint64_t operation = 0; operation < operations_per_thread; ++operation) {
            const bool is_write =
                int(bench::next_random(random_state) % 1000u) < writes_per_thousand;
            if (is_write)
                guarded.write(write_body);
            else
                guarded.read([&](const payload& value) { local_sink += read_body(value); });
        }
        global_sink.fetch_add(local_sink, std::memory_order_relaxed);
    });
}

// The library helper, wrapped in the same read/write shape.
struct library_wrapper {
    library_value value{};

    template <class Body>
    void read(Body body) const {
        auto held = value.lock_shared();
        body(*held);
    }
    template <class Body>
    void write(Body body) {
        auto held = value.lock();
        body(*held);
    }
};

double median_of(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

template <class Value>
double repeated(int thread_count, int writes_per_thousand) {
    Value guarded{};
    std::vector<double> samples;
    for (int repetition = 0; repetition < 1; ++repetition)
        bench_variant(guarded, thread_count, writes_per_thousand);
    for (int repetition = 0; repetition < 5; ++repetition)
        samples.push_back(bench_variant(guarded, thread_count, writes_per_thousand));
    return median_of(std::move(samples));
}

}

int main() {
    std::printf("Apple M3 Pro, 6 performance + 6 efficiency cores, g++-16 -O2\n");
    std::printf("%llu operations per thread, median of 5 runs after 1 warmup\n\n",
                (unsigned long long)operations_per_thread);

    std::printf("%-8s %-8s %14s %14s %14s   %s\n", "threads", "write%",
                "sync_value", "hand mutex", "hand shared", "verdict");
    for (int thread_count : {1, 2, 4, 8, 12}) {
        for (int writes_per_thousand : {0, 1, 10, 50, 200, 500, 1000}) {
            const double library =
                repeated<library_wrapper>(thread_count, writes_per_thousand);
            const double plain_mutex =
                repeated<hand_rolled_mutex_value>(thread_count, writes_per_thousand);
            const double shared_mutex =
                repeated<hand_rolled_shared_mutex_value>(thread_count, writes_per_thousand);
            const char* verdict = plain_mutex < library ? "plain mutex WINS" : "shared wins";
            std::printf("%-8d %-8.1f %14.2f %14.2f %14.2f   %s (%+.1f%%)\n",
                        thread_count, writes_per_thousand / 10.0, library,
                        plain_mutex, shared_mutex, verdict,
                        100.0 * (library - plain_mutex) / plain_mutex);
        }
        std::printf("\n");
    }
    std::printf("sink=%llu\n", (unsigned long long)global_sink.load());
}
