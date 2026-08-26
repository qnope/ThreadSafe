// Program 6 -- LATENCY/THROUGHPUT MICROBENCHMARK of synchronized_value.
// N threads, 90% reads / 10% writes on a std::map<int,int>, three ways:
//   1. threadsafe::synchronized_value<std::map<int,int>>  (picks shared_mutex)
//   2. the same map behind a hand-written std::shared_mutex
//   3. the same map behind a hand-written std::mutex
//
// build: g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread \
//            p6_bench.cpp -o p6 && ./p6
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {

constexpr int operations_per_thread = 200'000;
constexpr int key_space = 4096;

struct fast_random {
    std::uint64_t state;
    std::uint64_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
};

using guarded_map = threadsafe::synchronized_value<std::map<int, int>>;

struct library_worker {
    std::shared_ptr<guarded_map> table;
    std::shared_ptr<std::atomic<std::uint64_t>> checksum;
    int seed;

    void operator()() const {
        fast_random random{static_cast<std::uint64_t>(seed) * 2654435761u + 1};
        std::uint64_t local = 0;
        for (int operation = 0; operation < operations_per_thread; ++operation) {
            const std::uint64_t draw = random.next();
            const int key = static_cast<int>(draw % key_space);
            if (draw % 10 != 0) {
                const auto guard = table->lock_shared();
                const auto found = guard->find(key);
                if (found != guard->end())
                    local += static_cast<std::uint64_t>(found->second);
            } else {
                auto guard = table->lock();
                (*guard)[key] = key * 2;
            }
        }
        *checksum += local;
    }
};

template <class Mutex>
struct hand_written {
    Mutex mutex;
    std::map<int, int> table;
    std::atomic<std::uint64_t> checksum{0};
};

template <class Mutex, class ReadLock>
double run_hand_written(int thread_count) {
    hand_written<Mutex> shared;
    for (int key = 0; key < key_space; key += 2)
        shared.table[key] = key * 2;

    const auto started_at = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> threads;
        for (int worker = 0; worker < thread_count; ++worker)
            threads.emplace_back([&shared, worker] {
                fast_random random{
                    static_cast<std::uint64_t>(worker) * 2654435761u + 1};
                std::uint64_t local = 0;
                for (int operation = 0; operation < operations_per_thread;
                     ++operation) {
                    const std::uint64_t draw = random.next();
                    const int key = static_cast<int>(draw % key_space);
                    if (draw % 10 != 0) {
                        ReadLock lock{shared.mutex};
                        const auto found = shared.table.find(key);
                        if (found != shared.table.end())
                            local += static_cast<std::uint64_t>(found->second);
                    } else {
                        std::unique_lock lock{shared.mutex};
                        shared.table[key] = key * 2;
                    }
                }
                shared.checksum += local;
            });
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return std::chrono::duration<double, std::nano>(elapsed).count()
         / (double(thread_count) * operations_per_thread);
}

double run_library(int thread_count) {
    auto table = guarded_map::make();
    {
        auto guard = table->lock();
        for (int key = 0; key < key_space; key += 2)
            (*guard)[key] = key * 2;
    }
    auto checksum = std::make_shared<std::atomic<std::uint64_t>>(0);

    const auto started_at = std::chrono::steady_clock::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < thread_count; ++worker)
            launcher.launch_task(library_worker{table, checksum, worker});
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return std::chrono::duration<double, std::nano>(elapsed).count()
         / (double(thread_count) * operations_per_thread);
}

}

static_assert(threadsafe::is_synchronizable_v<const std::map<int, int>>,
              "which is why the wrapper picks a shared_mutex");
static_assert(std::is_same_v<guarded_map::mutex, std::shared_mutex>);
static_assert(std::is_same_v<guarded_map::const_guard,
                             threadsafe::value_guard<
                                 const std::map<int, int>,
                                 std::shared_lock<std::shared_mutex>>>);

int main() {
    std::printf("90%% read / 10%% write on std::map<int,int>, "
                "%d ops per thread, ns per op\n\n", operations_per_thread);
    std::printf("%8s %14s %14s %14s\n", "threads", "sync_value",
                "shared_mutex", "mutex");
    for (int thread_count : {1, 2, 4, 8, 12}) {
        const double library = run_library(thread_count);
        const double shared = run_hand_written<std::shared_mutex,
                                               std::shared_lock<std::shared_mutex>>(
            thread_count);
        const double exclusive =
            run_hand_written<std::mutex, std::unique_lock<std::mutex>>(
                thread_count);
        std::printf("%8d %14.1f %14.1f %14.1f\n", thread_count, library, shared,
                    exclusive);
    }
}
