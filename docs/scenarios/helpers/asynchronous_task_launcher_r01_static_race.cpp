#include <threadsafe/threadsafe.h>

#include <cstdio>

namespace {
// No captures, no data members: reflection sees an empty type, so every trait
// says yes. All the mutable state lives in a static member the walk never looks at.
struct RaceOnStatic {
    static inline long long shared_counter = 0;
    void operator()() const {
        for (int index = 0; index < 1'000'000; ++index)
            shared_counter = shared_counter + 1;
    }
};

// A namespace-scope launcher reached from a CAPTURELESS lambda: the closure is
// empty, so is_sendable / is_lifetime_aware both say yes.
threadsafe::asynchronous_task_launcher shared_launcher;
long long recursion_budget = 4;
}

static_assert(threadsafe::launchable_task<RaceOnStatic>,
              "the launcher accepts a callable whose only state is static");

int main() {
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int index = 0; index < 8; ++index)
            launcher.launch_task(RaceOnStatic{});
    }
    std::printf("expected 8000000, got %lld\n", RaceOnStatic::shared_counter);
    return RaceOnStatic::shared_counter == 8'000'000 ? 0 : 1;
}
