// Isolates the cost: is the crossover result the library's fault or the platform's
// pthread_rwlock? Measures (a) the uncontended single-thread cost of each policy
// through synchronized_value, and (b) the bare std:: primitives at 4 threads.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t iterations = 2'000'000;
std::atomic<long> sink{0};

template <class Callable>
double time_ns(std::size_t total_operations, Callable callable) {
    auto begin = std::chrono::steady_clock::now();
    callable();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(end - begin).count() / double(total_operations);
}

}

int main() {
    // (a) single thread, no contention at all.
    {
        threadsafe::synchronized_value<std::vector<int>> auto_selected{std::size_t{64}, 1};
        std::mutex plain_mutex;
        std::vector<int> plain_data(64, 1);

        double shared_read = time_ns(iterations, [&] {
            long local = 0;
            for (std::size_t i = 0; i < iterations; ++i) {
                auto guard = auto_selected.lock_shared();
                local += (*guard)[0];
            }
            sink.fetch_add(local, std::memory_order_relaxed);
        });
        double shared_write = time_ns(iterations, [&] {
            for (std::size_t i = 0; i < iterations; ++i) {
                auto guard = auto_selected.lock();
                (*guard)[0] += 1;
            }
        });
        double mutex_read = time_ns(iterations, [&] {
            long local = 0;
            for (std::size_t i = 0; i < iterations; ++i) {
                std::unique_lock lock{plain_mutex};
                local += plain_data[0];
            }
            sink.fetch_add(local, std::memory_order_relaxed);
        });
        std::printf("UNCONTENDED (1 thread, %zu iterations)\n", iterations);
        std::printf("  synchronized_value<vector<int>>.lock_shared() : %6.2f ns/op\n", shared_read);
        std::printf("  synchronized_value<vector<int>>.lock()        : %6.2f ns/op\n", shared_write);
        std::printf("  hand-written std::mutex + unique_lock         : %6.2f ns/op\n\n", mutex_read);
    }

    // (b) bare primitives, 4 threads, empty critical section.
    {
        constexpr unsigned thread_count = 4;
        constexpr std::size_t per_thread = 200'000;
        std::shared_mutex shared_primitive;
        std::mutex mutex_primitive;

        auto drive = [&](auto&& body) {
            std::atomic<bool> go{false};
            std::vector<std::jthread> workers;
            for (unsigned t = 0; t < thread_count; ++t)
                workers.emplace_back([&] {
                    while (!go.load(std::memory_order_acquire)) {}
                    for (std::size_t i = 0; i < per_thread; ++i) body();
                });
            auto begin = std::chrono::steady_clock::now();
            go.store(true, std::memory_order_release);
            workers.clear();
            auto end = std::chrono::steady_clock::now();
            return std::chrono::duration<double, std::nano>(end - begin).count()
                 / double(per_thread * thread_count);
        };

        double shared_shared = drive([&] { std::shared_lock lock{shared_primitive}; });
        double shared_unique = drive([&] { std::unique_lock lock{shared_primitive}; });
        double plain_unique  = drive([&] { std::unique_lock lock{mutex_primitive}; });
        std::printf("BARE PRIMITIVES, 4 threads, EMPTY critical section\n");
        std::printf("  std::shared_lock<std::shared_mutex> : %8.1f ns/op\n", shared_shared);
        std::printf("  std::unique_lock<std::shared_mutex> : %8.1f ns/op\n", shared_unique);
        std::printf("  std::unique_lock<std::mutex>        : %8.1f ns/op\n", plain_unique);
    }
    std::printf("sink=%ld\n", sink.load());
}
