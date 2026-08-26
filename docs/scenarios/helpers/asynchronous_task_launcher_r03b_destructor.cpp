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

// Polls the stop token, but only between 300 ms slices -- a realistic granularity
// for a task that does a chunk of work before checking.
struct PollsCoarsely {
    int identifier;
    void operator()(std::stop_token token) const {
        while (!token.stop_requested())
            std::this_thread::sleep_for(300ms);
        std::printf("[%7.1f ms] cooperative task %d observed the stop\n",
                    elapsed_ms(), identifier);
    }
};

// Ignores the token entirely; the traits have nothing to say about that.
struct IgnoresTheToken {
    int identifier;
    void operator()() const {
        std::this_thread::sleep_for(400ms);
        std::printf("[%7.1f ms] uncooperative task %d ran to completion\n",
                    elapsed_ms(), identifier);
    }
};
}

static_assert(threadsafe::launchable_task<PollsCoarsely>);
static_assert(threadsafe::launchable_task<IgnoresTheToken>);

int main() {
    program_start = clock_type::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int identifier = 0; identifier < 4; ++identifier)
            launcher.launch_task(PollsCoarsely{identifier});
        std::printf("[%7.1f ms] all 4 cooperative tasks launched; leaving scope\n", elapsed_ms());
    }
    std::printf("[%7.1f ms] destructor returned  (one stop-and-join at a time:\n"
                "           the latency is the SUM, not the MAX)\n\n", elapsed_ms());

    program_start = clock_type::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int identifier = 0; identifier < 4; ++identifier)
            launcher.launch_task(IgnoresTheToken{identifier});
        std::printf("[%7.1f ms] all 4 uncooperative tasks launched; leaving scope\n", elapsed_ms());
    }
    std::printf("[%7.1f ms] destructor returned\n", elapsed_ms());
}
