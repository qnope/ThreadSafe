#include <threadsafe/threadsafe.h>
#include <cstdio>

namespace { long long global_total = 0; }

// No static data member anywhere. Purely a namespace-scope global.
struct GlobalCounter {
    int weight = 1;
    void operator()() const {
        for (int iteration = 0; iteration < 200000; ++iteration)
            global_total += weight;
    }
};

static_assert(threadsafe::is_sendable_v<GlobalCounter>);
static_assert(threadsafe::is_lifetime_aware_v<GlobalCounter>);
static_assert(threadsafe::launchable_task<GlobalCounter>);

int main() {
    {
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(GlobalCounter{1});
        launcher.launch_task(GlobalCounter{2});
    }
    std::printf("global_total = %lld (expected 600000)\n", global_total);
}
