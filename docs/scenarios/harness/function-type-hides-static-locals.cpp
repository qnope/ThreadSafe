#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <string>

// The library refuses a closure with captures because it "holds state
// reflection cannot see". A function-local static is exactly that same
// invisible mutable state, but is_synchronizable<F> says yes to every function
// type, so every function pointer sails through.
long next_id() {
    static long counter = 0;      // invisible, mutable, shared
    return ++counter;
}

static_assert(threadsafe::is_synchronizable_v<decltype(next_id)>);
static_assert(threadsafe::is_sendable_v<decltype(&next_id)>);
static_assert(threadsafe::is_lifetime_aware_v<decltype(&next_id)>);
static_assert(threadsafe::launchable_task<decltype(&next_id)>);

// For contrast, a closure holding the same counter is refused outright.
using Counting = decltype([n = 0L]() mutable { return ++n; });
static_assert(!threadsafe::is_sendable_v<Counting>,
              "holds state reflection cannot see");

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(&next_id);
    launcher.launch_task(&next_id);
    std::printf("ok\n");
}
