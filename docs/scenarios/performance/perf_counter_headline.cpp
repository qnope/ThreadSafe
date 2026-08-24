// The canonical demo: a shared counter. synchronized_value<int> auto-selects
// shared_mutex; the obvious hand-written version uses a mutex.
#include <threadsafe/threadsafe.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {
constexpr std::size_t per_thread = 100'000;

template <class Body>
double drive(unsigned thread_count, Body body) {
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
}
}

int main() {
    std::printf("sizeof(synchronized_value<int>) = %zu   (mutex+int would be %zu)\n\n",
                sizeof(threadsafe::synchronized_value<int>),
                sizeof(std::mutex) + sizeof(int) + 4);
    std::printf("%8s %16s %14s %10s\n", "threads", "sync_value<int>", "mutex+int", "speedup");
    for (unsigned thread_count : {1u, 2u, 4u, 8u, 12u}) {
        threadsafe::synchronized_value<int> counter{0};
        struct { std::mutex mutex_; int value_ = 0; } plain;
        double best_sync = 1e30, best_plain = 1e30;
        for (int r = 0; r < 3; ++r) {
            double s = drive(thread_count, [&] { auto guard = counter.lock(); *guard += 1; });
            double p = drive(thread_count, [&] { std::unique_lock l{plain.mutex_}; plain.value_ += 1; });
            best_sync = s < best_sync ? s : best_sync;
            best_plain = p < best_plain ? p : best_plain;
        }
        std::printf("%8u %13.1f ns %11.1f ns %9.2fx\n",
                    thread_count, best_sync, best_plain, best_plain / best_sync);
    }
}
