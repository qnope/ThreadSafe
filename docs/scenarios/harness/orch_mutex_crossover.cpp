// Independent check: does synchronized_value's automatic shared_mutex actually
// lose to a plain std::mutex on a short critical section?
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <numeric>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {
constexpr int kOpsPerThread = 30000;
constexpr int kPayload = 64;

template <class Mutex, class ReadOp, class WriteOp>
double run(int threads, int read_percent, Mutex& mutex, ReadOp read_op, WriteOp write_op) {
    std::atomic<bool> go{false};
    std::vector<std::jthread> workers;
    auto start = std::chrono::steady_clock::now();
    for (int t = 0; t < threads; ++t)
        workers.emplace_back([&, t] {
            while (!go.load(std::memory_order_acquire)) {}
            for (int i = 0; i < kOpsPerThread; ++i)
                if ((i * 100 / kOpsPerThread) < read_percent) read_op();
                else write_op();
        });
    start = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_release);
    workers.clear();
    auto stop = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(stop - start).count()
           / (threads * kOpsPerThread);
}
}

int main() {
    std::printf(" threads  read%%   shared_mutex        mutex   speedup\n");
    for (int threads : {2, 4, 8, 12})
        for (int read_percent : {0, 50, 90, 99}) {
            std::vector<int> data(kPayload, 1);
            std::shared_mutex smutex;
            double shared_ns = 1e18;
            for (int rep = 0; rep < 3; ++rep) shared_ns = std::min(shared_ns, run(threads, read_percent, smutex,
                [&] { std::shared_lock lock(smutex); volatile int s = std::accumulate(data.begin(), data.end(), 0); (void)s; },
                [&] { std::unique_lock lock(smutex); ++data[0]; }));
            std::mutex mmutex;
            double mutex_ns = 1e18;
            for (int rep = 0; rep < 3; ++rep) mutex_ns = std::min(mutex_ns, run(threads, read_percent, mmutex,
                [&] { std::lock_guard lock(mmutex); volatile int s = std::accumulate(data.begin(), data.end(), 0); (void)s; },
                [&] { std::lock_guard lock(mmutex); ++data[0]; }));
            std::printf("%8d %6d%%  %11.1f ns %10.1f ns   %6.2fx\n",
                        threads, read_percent, shared_ns, mutex_ns, mutex_ns / shared_ns);
        }
}
