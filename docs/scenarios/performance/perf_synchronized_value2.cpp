#include <threadsafe/threadsafe.h>

#include <algorithm>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

constexpr int payload_words = 4096;

struct payload {
    int counters[payload_words];
};

static_assert(threadsafe::is_synchronizable_v<const payload>);
using library_value = threadsafe::synchronized_value<payload>;
static_assert(std::is_same_v<library_value::mutex, std::shared_mutex>,
              "the library picks shared_mutex automatically for this payload");

std::atomic<std::uint64_t> global_sink{0};

inline std::uint64_t read_body(const payload& value, int words) {
    std::uint64_t total = 0;
    for (int index = 0; index < words; ++index)
        total += std::uint64_t(value.counters[index]);
    return total;
}
inline void write_body(payload& value, int words) {
    for (int index = 0; index < words; ++index)
        ++value.counters[index];
}

struct library_wrapper {
    library_value value{};
    template <class Body> void read(Body body) const {
        auto held = value.lock_shared(); body(*held);
    }
    template <class Body> void write(Body body) {
        auto held = value.lock(); body(*held);
    }
    static const char* name() { return "synchronized_value (shared_mutex)"; }
};

struct plain_mutex_wrapper {
    mutable std::mutex mutex;
    payload value{};
    template <class Body> void read(Body body) const {
        std::lock_guard<std::mutex> held{mutex}; body(value);
    }
    template <class Body> void write(Body body) {
        std::lock_guard<std::mutex> held{mutex}; body(value);
    }
    static const char* name() { return "hand-rolled std::mutex"; }
};

struct shared_mutex_wrapper {
    mutable std::shared_mutex mutex;
    payload value{};
    template <class Body> void read(Body body) const {
        std::shared_lock<std::shared_mutex> held{mutex}; body(value);
    }
    template <class Body> void write(Body body) {
        std::lock_guard<std::shared_mutex> held{mutex}; body(value);
    }
    static const char* name() { return "hand-rolled std::shared_mutex"; }
};

constexpr int warmup_repetitions = 1;
constexpr int measured_repetitions = 5;

// Threads are created once, outside every timed window; a barrier opens and
// closes each measured repetition.
template <class Wrapper>
double run(int thread_count, int writes_per_thousand, int words,
           std::uint64_t operations_per_thread) {
    Wrapper guarded{};
    std::barrier open_gate{thread_count + 1};
    std::barrier close_gate{thread_count + 1};
    std::vector<double> samples;

    {
        std::vector<std::jthread> workers;
        workers.reserve(thread_count);
        for (int thread_index = 0; thread_index < thread_count; ++thread_index)
            workers.emplace_back([&, thread_index] {
                std::uint32_t random_state =
                    0x9e3779b9u + std::uint32_t(thread_index) * 2654435761u;
                for (int repetition = 0;
                     repetition < warmup_repetitions + measured_repetitions;
                     ++repetition) {
                    open_gate.arrive_and_wait();
                    std::uint64_t local_sink = 0;
                    for (std::uint64_t operation = 0;
                         operation < operations_per_thread; ++operation) {
                        random_state ^= random_state << 13;
                        random_state ^= random_state >> 17;
                        random_state ^= random_state << 5;
                        if (int(random_state % 1000u) < writes_per_thousand)
                            guarded.write([&](payload& value) { write_body(value, words); });
                        else
                            guarded.read([&](const payload& value) {
                                local_sink += read_body(value, words);
                            });
                    }
                    global_sink.fetch_add(local_sink, std::memory_order_relaxed);
                    close_gate.arrive_and_wait();
                }
            });

        for (int repetition = 0;
             repetition < warmup_repetitions + measured_repetitions; ++repetition) {
            open_gate.arrive_and_wait();
            const auto start = std::chrono::steady_clock::now();
            close_gate.arrive_and_wait();
            const auto stop = std::chrono::steady_clock::now();
            if (repetition >= warmup_repetitions)
                samples.push_back(
                    std::chrono::duration<double, std::nano>(stop - start).count()
                    / double(std::uint64_t(thread_count) * operations_per_thread));
        }
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

void sweep(int words, std::uint64_t operations_per_thread) {
    std::printf("=== critical section = %d int%s (%zu bytes touched), "
                "%llu ops/thread, median of %d ===\n",
                words, words == 1 ? "" : "s", std::size_t(words) * sizeof(int),
                (unsigned long long)operations_per_thread, measured_repetitions);
    std::printf("%-8s %-8s %14s %14s %14s   %s\n", "threads", "write%",
                "sync_value", "std::mutex", "shared_mutex", "mutex/shared speedup");
    for (int thread_count : {1, 2, 4, 8, 12}) {
        for (int writes_per_thousand : {0, 1, 10, 100, 500, 1000}) {
            const double library = run<library_wrapper>(
                thread_count, writes_per_thousand, words, operations_per_thread);
            const double plain = run<plain_mutex_wrapper>(
                thread_count, writes_per_thousand, words, operations_per_thread);
            const double shared = run<shared_mutex_wrapper>(
                thread_count, writes_per_thousand, words, operations_per_thread);
            std::printf("%-8d %-8.1f %14.2f %14.2f %14.2f   %6.2fx %s\n",
                        thread_count, writes_per_thousand / 10.0,
                        library, plain, shared,
                        library / plain,
                        library > plain ? "(plain mutex faster)" : "(shared faster)");
        }
        std::printf("\n");
    }
}

}

int main() {
    std::printf("Apple M3 Pro (6 performance + 6 efficiency cores), g++-16 -O2, "
                "libstdc++ shared_mutex == pthread_rwlock_t\n");
    std::printf("Thread creation happens outside every timed window.\n\n");
    sweep(8, 100000);
    sweep(256, 20000);
    sweep(4096, 2000);
    std::printf("sink=%llu\n", (unsigned long long)global_sink.load());
}
