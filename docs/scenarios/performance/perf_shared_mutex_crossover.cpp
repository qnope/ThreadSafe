// Q2: when does the shared_mutex that synchronized_value auto-selects actually WIN?
// Same payload, same critical sections, only the mutex policy differs.
//   A) threadsafe::synchronized_value<std::vector<int>>  -> auto-selected std::shared_mutex
//   B) hand-written std::mutex + std::vector<int>        -> everything under a unique_lock
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t payload_size = 64;
constexpr std::size_t operations_per_thread = 30'000;
constexpr int repetitions = 3;

struct mutex_protected_vector {
    mutable std::mutex mutex_;
    std::vector<int> data_;
    explicit mutex_protected_vector(std::size_t size) : data_(size, 1) {}
};

inline std::uint32_t next_random(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

// Same body for both policies: a read sums the payload, a write bumps one slot.
inline long read_payload(const std::vector<int>& data) {
    long total = 0;
    for (std::size_t index = 0; index < payload_size; ++index) total += data[index];
    return total;
}

inline void write_payload(std::vector<int>& data, std::uint32_t seed) {
    data[seed % payload_size] += 1;
}

std::atomic<long> sink{0};

template <class Body>
double run_once(unsigned thread_count, Body body) {
    std::atomic<bool> go{false};
    std::vector<std::jthread> workers;
    workers.reserve(thread_count);
    const auto start_barrier = [&] { while (!go.load(std::memory_order_acquire)) {} };

    auto begin = std::chrono::steady_clock::now();
    for (unsigned identifier = 0; identifier < thread_count; ++identifier)
        workers.emplace_back([&, identifier] {
            start_barrier();
            body(identifier);
        });
    begin = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_release);
    workers.clear();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(end - begin).count()
         / double(operations_per_thread * thread_count);
}

double bench_shared(unsigned thread_count, unsigned read_percent) {
    threadsafe::synchronized_value<std::vector<int>> shared_value{payload_size, 1};
    double best = 1e30;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        double nanoseconds = run_once(thread_count, [&](unsigned identifier) {
            std::uint32_t state = 0x9E3779B9u + identifier * 2654435761u;
            long local = 0;
            for (std::size_t operation = 0; operation < operations_per_thread; ++operation) {
                std::uint32_t random = next_random(state);
                if (random % 100u < read_percent) {
                    auto guard = shared_value.lock_shared();
                    local += read_payload(*guard);
                } else {
                    auto guard = shared_value.lock();
                    write_payload(*guard, random);
                }
            }
            sink.fetch_add(local, std::memory_order_relaxed);
        });
        best = nanoseconds < best ? nanoseconds : best;
    }
    return best;
}

double bench_exclusive(unsigned thread_count, unsigned read_percent) {
    mutex_protected_vector guarded{payload_size};
    double best = 1e30;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        double nanoseconds = run_once(thread_count, [&](unsigned identifier) {
            std::uint32_t state = 0x9E3779B9u + identifier * 2654435761u;
            long local = 0;
            for (std::size_t operation = 0; operation < operations_per_thread; ++operation) {
                std::uint32_t random = next_random(state);
                if (random % 100u < read_percent) {
                    std::unique_lock lock{guarded.mutex_};
                    local += read_payload(guarded.data_);
                } else {
                    std::unique_lock lock{guarded.mutex_};
                    write_payload(guarded.data_, random);
                }
            }
            sink.fetch_add(local, std::memory_order_relaxed);
        });
        best = nanoseconds < best ? nanoseconds : best;
    }
    return best;
}

}

int main() {
    static_assert(std::is_same_v<threadsafe::synchronized_value<std::vector<int>>::mutex,
                                 std::shared_mutex>,
                  "synchronized_value<vector<int>> must auto-select shared_mutex");

    std::printf("payload = %zu ints summed per read, %zu ops/thread, best of %d\n\n",
                payload_size, operations_per_thread, repetitions);
    std::printf("%8s %8s %14s %14s %10s\n",
                "threads", "read%", "shared_mutex", "mutex", "speedup");
    for (unsigned thread_count : {2u, 4u, 8u, 12u}) {
        for (unsigned read_percent : {0u, 50u, 90u, 99u}) {
            double shared_nanoseconds = bench_shared(thread_count, read_percent);
            double exclusive_nanoseconds = bench_exclusive(thread_count, read_percent);
            std::printf("%8u %7u%% %12.1f ns %12.1f ns %9.2fx\n",
                        thread_count, read_percent, shared_nanoseconds,
                        exclusive_nanoseconds, exclusive_nanoseconds / shared_nanoseconds);
        }
        std::printf("\n");
    }
    std::printf("sink=%ld\n", sink.load());
}
