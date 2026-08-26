#include <chrono>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>
using clk = std::chrono::steady_clock;
static constexpr int kOps = 40000;
static volatile double sink_value = 0;

static void work(int spins) { double acc = 0; for (int i = 0; i < spins; ++i) acc += i * 0.5; sink_value = acc; }

template <class Mutex, class ReadLock>
double bench(int threads, int read_percent, int spins) {
    Mutex mutex; long value = 0;
    auto start = clk::now();
    { std::vector<std::jthread> workers;
      for (int t = 0; t < threads; ++t)
        workers.emplace_back([&, t] {
            unsigned rng = 999u + t;
            for (int i = 0; i < kOps; ++i) {
                rng = rng * 1103515245u + 12345u;
                if (int(rng >> 16) % 100 < read_percent) { ReadLock lock(mutex); work(spins); (void)value; }
                else { std::unique_lock lock(mutex); work(spins); ++value; }
            }
        });
    }
    return std::chrono::duration<double, std::nano>(clk::now() - start).count() / (threads * kOps);
}
int main() {
    std::printf("4 threads, 90%% reads — critical section length vs shared_mutex payoff\n");
    std::printf("%-10s %12s %12s %8s\n", "spins", "shared_mtx", "std::mutex", "ratio");
    for (int spins : {0, 50, 200, 800, 3200, 12800}) {
        double s = bench<std::shared_mutex, std::shared_lock<std::shared_mutex>>(4, 90, spins);
        double m = bench<std::mutex, std::unique_lock<std::mutex>>(4, 90, spins);
        std::printf("%-10d %11.0fns %11.0fns %7.2fx %s\n", spins, s, m, s/m, (s<m?"<-- shared_mutex wins":""));
    }
}
