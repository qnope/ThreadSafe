// Where is the crossover? Same 90/10 map workload, 8 threads, but the read
// walks `read_span` map entries so the critical section gets longer.
// build: g++-16 -std=c++26 -O2 -pthread p6b_crossover.cpp -o p6b
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {
constexpr int operations_per_thread = 100'000;
constexpr int key_space = 4096;
constexpr int thread_count = 8;

struct fast_random {
    std::uint64_t state;
    std::uint64_t next() {
        state ^= state << 13; state ^= state >> 7; state ^= state << 17;
        return state;
    }
};

template <class Mutex, class ReadLock>
double run(int read_span) {
    Mutex mutex;
    std::map<int, int> table;
    std::atomic<std::uint64_t> checksum{0};
    for (int key = 0; key < key_space; key += 2) table[key] = key * 2;

    const auto started_at = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> threads;
        for (int worker = 0; worker < thread_count; ++worker)
            threads.emplace_back([&, worker] {
                fast_random random{std::uint64_t(worker) * 2654435761u + 1};
                std::uint64_t local = 0;
                for (int op = 0; op < operations_per_thread; ++op) {
                    const std::uint64_t draw = random.next();
                    const int key = int(draw % key_space);
                    if (draw % 10 != 0) {
                        ReadLock lock{mutex};
                        auto it = table.lower_bound(key);
                        for (int step = 0; step < read_span && it != table.end();
                             ++step, ++it)
                            local += std::uint64_t(it->second);
                    } else {
                        std::unique_lock lock{mutex};
                        table[key] = key * 2;
                    }
                }
                checksum += local;
            });
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return std::chrono::duration<double, std::nano>(elapsed).count()
         / (double(thread_count) * operations_per_thread);
}
}

int main() {
    std::printf("%12s %14s %14s %10s\n", "read_span", "shared_mutex", "mutex",
                "winner");
    for (int read_span : {1, 4, 16, 64, 256, 1024}) {
        const double shared =
            run<std::shared_mutex, std::shared_lock<std::shared_mutex>>(read_span);
        const double exclusive =
            run<std::mutex, std::unique_lock<std::mutex>>(read_span);
        std::printf("%12d %14.1f %14.1f %10s\n", read_span, shared, exclusive,
                    shared < exclusive ? "shared" : "mutex");
    }
}
