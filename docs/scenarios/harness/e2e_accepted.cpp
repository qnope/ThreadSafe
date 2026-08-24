// End-to-end positive check: everything the launcher ACCEPTS is exercised
// concurrently for real, under ThreadSanitizer. If the traits are sound, this
// program must be race-free.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Counter {
    std::atomic<int> hits{0};
};

}
template <>
struct threadsafe::is_synchronizable<Counter> : std::true_type {};

namespace {

using threadsafe::asynchronous_task_launcher;
using threadsafe::copy_on_write;
using threadsafe::synchronized_value;

constexpr int kTasks = 8;
constexpr int kIterations = 2000;

void every_accepted_shape_runs_concurrently() {
    auto counter = std::make_shared<Counter>();
    auto guarded_total = std::make_shared<synchronized_value<long>>(0L);
    auto guarded_index = std::make_shared<synchronized_value<std::map<int, std::string>>>();
    copy_on_write<std::vector<int>> shared_config(std::size_t{64}, 7);

    {
        asynchronous_task_launcher launcher;

        // 1. shared_ptr to a synchronizable user type
        for (int task = 0; task < kTasks; ++task)
            launcher.launch_task(
                [](std::shared_ptr<Counter> shared_counter) {
                    for (int i = 0; i < kIterations; ++i)
                        shared_counter->hits.fetch_add(1, std::memory_order_relaxed);
                },
                counter);

        // 2. shared_ptr to a synchronized_value: the checked mutable-sharing path
        for (int task = 0; task < kTasks; ++task)
            launcher.launch_task(
                [](std::shared_ptr<synchronized_value<long>> total) {
                    for (int i = 0; i < kIterations; ++i) {
                        auto guard = total->lock();
                        *guard += 1;
                    }
                },
                guarded_total);

        // 3. a shared_mutex-backed value read by many, written by few
        for (int task = 0; task < kTasks; ++task)
            launcher.launch_task(
                [](std::shared_ptr<synchronized_value<std::map<int, std::string>>> index,
                   int seed) {
                    for (int i = 0; i < 200; ++i) {
                        if (i % 10 == 0) {
                            auto guard = index->lock();
                            (*guard)[seed * 1000 + i] = std::to_string(i);
                        } else {
                            auto guard = index->lock_shared();
                            (void)guard->size();
                        }
                    }
                },
                guarded_index, task);

        // 4. copy_on_write shared by value: unsynchronised reads, no lock at all
        for (int task = 0; task < kTasks; ++task)
            launcher.launch_task(
                [](copy_on_write<std::vector<int>> config,
                   std::shared_ptr<Counter> shared_counter) {
                    long sum = 0;
                    for (int i = 0; i < kIterations; ++i)
                        sum += (*config)[i % config->size()];
                    if (sum == 0)
                        shared_counter->hits.fetch_add(1, std::memory_order_relaxed);
                },
                shared_config, counter);

        // 5. plain sendable values, and a plain function pointer
        for (int task = 0; task < kTasks; ++task)
            launcher.launch_task(
                [](int a, double b, std::string s, std::vector<int> v) {
                    (void)(a + static_cast<int>(b) + static_cast<int>(s.size())
                           + static_cast<int>(v.size()));
                },
                task, 1.5, std::string("payload"), std::vector<int>{1, 2, 3});
    }

    // The launcher joined everything in its destructor.
    const int expected = kTasks * kIterations;
    if (counter->hits.load() != expected) {
        std::printf("FAIL: counter = %d, expected %d\n",
                    counter->hits.load(), expected);
        std::abort();
    }
    auto total_guard = guarded_total->lock();
    if (*total_guard != static_cast<long>(kTasks) * kIterations) {
        std::printf("FAIL: total = %ld\n", *total_guard);
        std::abort();
    }
    auto index_guard = guarded_index->lock_shared();
    if (index_guard->size() != static_cast<std::size_t>(kTasks) * 20) {
        std::printf("FAIL: index size = %zu\n", index_guard->size());
        std::abort();
    }
}

}

int main() {
    every_accepted_shape_runs_concurrently();
    std::puts("end-to-end: every accepted shape ran race-free");
}
