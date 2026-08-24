#include <threadsafe/threadsafe.h>

#include <cstdio>

// Every special member is implicit, the only non-static data member is an int,
// there is no capture, no base, no pointer, no reference. is_sendable says yes.
// The state every copy actually works on is the static one, which the
// structural walk never looks at.
struct RequestCounter {
    static inline long long total = 0;

    int weight = 1;

    void operator()() const {
        for (int iteration = 0; iteration < 200000; ++iteration)
            total += weight;
    }
};

static_assert(threadsafe::is_sendable_v<RequestCounter>,
              "the trait accepts it");
static_assert(threadsafe::is_lifetime_aware_v<RequestCounter>,
              "and so does the ownership trait");
static_assert(threadsafe::launchable_task<RequestCounter>,
              "so the launcher accepts it too");

int main() {
    {
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(RequestCounter{1});
        launcher.launch_task(RequestCounter{2});
    }
    std::printf("total = %lld (expected 600000)\n", RequestCounter::total);
}