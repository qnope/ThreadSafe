#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stop_token>
#include <thread>

using namespace std::chrono_literals;

namespace {
// A perfectly ordinary cooperative task: it runs until it is asked to stop.
// std::jthread injects the stop_token, and the traits bless it (the class has
// the static_assert for exactly that).
struct RunsUntilStopped {
    void operator()(std::stop_token token) const {
        std::printf("task: waiting for a stop request...\n");
        std::fflush(stdout);
        while (!token.stop_requested())
            std::this_thread::sleep_for(10ms);
        std::printf("task: stop observed\n");
    }
};
}

static_assert(threadsafe::launchable_scoped_task<RunsUntilStopped>);

int main() {
    std::jthread watchdog{[](std::stop_token watchdog_token) {
        for (int slice = 0; slice < 200 && !watchdog_token.stop_requested(); ++slice)
            std::this_thread::sleep_for(10ms);
        if (!watchdog_token.stop_requested()) {
            std::printf("WATCHDOG: launch_scoped_task has not returned after 2 s -- deadlock\n");
            std::fflush(stdout);
            std::_Exit(42);
        }
    }};

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_scoped_task(RunsUntilStopped{});

    std::printf("launch_scoped_task returned\n");
    watchdog.request_stop();
}
