#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <stdexcept>

namespace {
struct ThrowsOnce {
    void operator()() const { throw std::runtime_error("task failed"); }
};
}

static_assert(threadsafe::launchable_scoped_task<ThrowsOnce>);

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    try {
        // launch_scoped_task blocks until the task ends, so the caller looks like
        // an ordinary synchronous call -- but the exception cannot come back.
        launcher.launch_scoped_task(ThrowsOnce{});
        std::printf("no exception\n");
    } catch (const std::exception &error) {
        std::printf("caught: %s\n", error.what());
        return 0;
    }
    return 1;
}
