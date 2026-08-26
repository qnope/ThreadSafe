#include <threadsafe/threadsafe.h>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>
using clk = std::chrono::steady_clock;
static constexpr int kOps = 200000;

template <class Mutex, class ReadLock>
double bench(int threads, int read_percent) {
    Mutex mutex; long value = 0; std::atomic<long> sink{0};
    auto start = clk::now();
    std::vector<std::jthread> workers;
    for (int t = 0; t < threads; ++t)
        workers.emplace_back([&, t] {
            unsigned rng = 12345u + t;
            for (int i = 0; i < kOps; ++i) {
                rng = rng * 1103515245u + 12345u;
                if (int(rng >> 16) % 100 < read_percent) { ReadLock lock(mutex); sink += value; }
                else { std::unique_lock lock(mutex); ++value; }
            }
        });
    workers.clear();
    return std::chrono::duration<double, std::nano>(clk::now() - start).count() / (threads * kOps);
}
int main() {
    std::printf("%-8s %-14s %10s %10s %8s\n", "threads", "read%", "shared_mtx", "std::mutex", "ratio");
    for (int threads : {2, 4, 8})
        for (int rp : {50, 90, 99}) {
            double s = bench<std::shared_mutex, std::shared_lock<std::shared_mutex>>(threads, rp);
            double m = bench<std::mutex, std::unique_lock<std::mutex>>(threads, rp);
            std::printf("%-8d %-14d %9.1fns %9.1fns %7.2fx\n", threads, rp, s, m, s / m);
        }
}
