#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstdio>
#include <memory>

struct Stats {          // plain aggregate of atomics, NO vouch anywhere
    std::atomic<long> hits;
    std::atomic<long> misses;
};

using SV = threadsafe::synchronized_value<Stats>;
static_assert(threadsafe::is_synchronizable_v<SV>);
static_assert(threadsafe::is_sendable_v<std::shared_ptr<SV>>);
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<SV>>);

void work(std::shared_ptr<SV> stats) {
    for (int i = 0; i < 200000; ++i) {
        auto held = stats->lock();
        held->hits.fetch_add(1, std::memory_order_relaxed);
    }
}

int main() {
    auto stats = SV::make();
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(&work, stats);
    launcher.launch_task(&work, stats);
    auto held = stats->lock_shared();
    std::printf("hits=%ld\n", held->hits.load());
}
