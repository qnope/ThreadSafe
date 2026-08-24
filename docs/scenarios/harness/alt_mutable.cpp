#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstdio>
#include <memory>

// The library-blessed idiom, NO UNSAFE macro: exactly test_synchronizable.cpp's
// `struct SafeCounter { mutable std::atomic<int> hits; };`
struct Stats {
    mutable std::atomic<long> hits;
    mutable std::atomic<long> misses;
};

// No vouch at all. The structural const walk answers.
static_assert(threadsafe::is_synchronizable_v<const Stats>);
static_assert(threadsafe::is_sendable_v<std::shared_ptr<const Stats>>);
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<const Stats>>);

void work(std::shared_ptr<const Stats> stats) {
    for (int i = 0; i < 200000; ++i)
        stats->hits.fetch_add(1, std::memory_order_relaxed);
}

int main() {
    auto stats = std::make_shared<const Stats>();
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(&work, stats);
    launcher.launch_task(&work, stats);
}
