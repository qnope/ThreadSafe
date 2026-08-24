// Q2 follow-up: with the read ratio pinned at 100% (the most favourable case a
// shared_mutex can ever get), how long must the read critical section be before
// the auto-selected shared_mutex beats a plain mutex? 4 threads.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr unsigned thread_count = 4;
std::atomic<long> sink{0};

template <class Body>
double drive(std::size_t per_thread, Body body) {
    std::atomic<bool> go{false};
    std::vector<std::jthread> workers;
    for (unsigned t = 0; t < thread_count; ++t)
        workers.emplace_back([&] {
            while (!go.load(std::memory_order_acquire)) {}
            long local = 0;
            for (std::size_t i = 0; i < per_thread; ++i) local += body();
            sink.fetch_add(local, std::memory_order_relaxed);
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
    std::printf("100%% reads, %u threads. read = sum of N ints under the lock.\n\n", thread_count);
    std::printf("%10s %14s %14s %10s\n", "N ints", "shared_mutex", "mutex", "speedup");
    for (std::size_t payload : {16u, 64u, 256u, 1024u, 4096u, 16384u, 65536u}) {
        const std::size_t per_thread = payload >= 4096 ? 20'000 : 200'000;
        threadsafe::synchronized_value<std::vector<int>> auto_selected{payload, 1};
        std::mutex plain_mutex;
        std::vector<int> plain_data(payload, 1);

        double shared_ns = 1e30, mutex_ns = 1e30;
        for (int repetition = 0; repetition < 3; ++repetition) {
            double s = drive(per_thread, [&] {
                auto guard = auto_selected.lock_shared();
                long total = 0;
                for (std::size_t i = 0; i < payload; ++i) total += (*guard)[i];
                return total;
            });
            double m = drive(per_thread, [&] {
                std::unique_lock lock{plain_mutex};
                long total = 0;
                for (std::size_t i = 0; i < payload; ++i) total += plain_data[i];
                return total;
            });
            shared_ns = s < shared_ns ? s : shared_ns;
            mutex_ns = m < mutex_ns ? m : mutex_ns;
        }
        std::printf("%10zu %12.1f ns %12.1f ns %9.2fx\n",
                    payload, shared_ns, mutex_ns, mutex_ns / shared_ns);
    }
    std::printf("\nsink=%ld\n", sink.load());
}
