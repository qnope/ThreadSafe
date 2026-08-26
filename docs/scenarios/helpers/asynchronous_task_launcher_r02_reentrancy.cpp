#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>

namespace {
// The launcher lives at namespace scope, so a CAPTURELESS lambda reaches it.
// A captureless closure is an empty type: is_sendable and is_lifetime_aware
// both say yes, and launchable_task is satisfied.
threadsafe::asynchronous_task_launcher shared_launcher;

std::atomic<int> tasks_started{0};

void reenter() {
    ++tasks_started;
    // Two tasks running this concurrently both push_back into shared_launcher's
    // std::vector<std::jthread>. No lock, no atomic: a data race, and a
    // reallocation under another thread's iterator.
    if (tasks_started.load() < 64)
        shared_launcher.launch_task([] { reenter(); });
}
}

static_assert(threadsafe::launchable_task<decltype([] { reenter(); })>,
              "the traits do NOT stop a task from launching into its own launcher");

int main() {
    for (int index = 0; index < 8; ++index)
        shared_launcher.launch_task([] { reenter(); });

    std::printf("main done, started=%d\n", tasks_started.load());
}
