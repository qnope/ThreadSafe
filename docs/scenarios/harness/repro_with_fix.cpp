#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <memory>

// Because a plain aggregate of atomics is not synchronizable, the only way to
// share one is the UNSAFE vouch. The vouch is unconditional and is never
// re-checked, so the day someone adds a plain member the trait keeps saying
// yes — the false negative pushed the user onto a rule that cannot rot-detect.
struct Stats {
    std::atomic<long> hits;
    std::atomic<long> misses;
    long backlog;          // added later; nobody re-ran the trait
};

THREADSAFE_SYNCHRONIZABLE_MEMBERS(Stats);

static_assert(threadsafe::is_synchronizable_v<Stats>);
static_assert(threadsafe::is_sendable_v<std::shared_ptr<Stats>>);
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<Stats>>);

void work(std::shared_ptr<Stats> stats) {
    for (int i = 0; i < 200000; ++i) {
        stats->hits.fetch_add(1, std::memory_order_relaxed);
        stats->backlog += 1;          // plain long, no synchronisation
    }
}

int main() {
    auto stats = std::make_shared<Stats>();
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(&work, stats);
    launcher.launch_task(&work, stats);
    std::printf("hits=%ld backlog=%ld\n", stats->hits.load(), stats->backlog);
}
