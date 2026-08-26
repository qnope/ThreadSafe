#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cstdio>
#include <stop_token>
#include <thread>

using namespace std::chrono_literals;
using clock_type = std::chrono::steady_clock;

namespace {
clock_type::time_point program_start;

double elapsed_ms() {
    return std::chrono::duration<double, std::milli>(clock_type::now() - program_start).count();
}

// A well-behaved task: it takes the stop_token jthread injects and polls it.
struct PollsStopToken {
    int identifier;
    void operator()(std::stop_token token) const {
        for (int slice = 0; slice < 40 && !token.stop_requested(); ++slice)
            std::this_thread::sleep_for(25ms);
        std::printf("[%7.1f ms] task %d finished, stop_requested=%d\n",
                    elapsed_ms(), identifier, int(token.stop_requested()));
    }
};
}

static_assert(threadsafe::launchable_task<PollsStopToken>);

int main() {
    program_start = clock_type::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int identifier = 0; identifier < 3; ++identifier)
            launcher.launch_task(PollsStopToken{identifier});
        std::printf("[%7.1f ms] leaving the scope; there is no request_stop(), no\n"
                    "           join_all(), no way to ask whether the tasks are done\n",
                    elapsed_ms());
    }
    std::printf("[%7.1f ms] destructor returned\n", elapsed_ms());
}
