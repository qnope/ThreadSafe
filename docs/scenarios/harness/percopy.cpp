#include <threadsafe/threadsafe.h>
#include <cstdio>

// The claim says the closure [n = 0L]() mutable holds "the identical counter"
// as next_id()'s static. It does not: the launcher takes the callable BY VALUE,
// so per-object state is duplicated per thread and cannot race. The library
// already accepts the declared-member spelling of exactly that closure.
struct Counting {
    long n = 0;
    long operator()() { return ++n; }
};

static_assert(threadsafe::is_sendable_v<Counting>);
static_assert(threadsafe::launchable_task<Counting>);

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    Counting counting;
    launcher.launch_task(counting);
    launcher.launch_task(counting);
    std::printf("ok\n");
}
