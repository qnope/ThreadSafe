// (a) the same accepted program without the artificial scope: the launcher has
// no join()/wait(), so the only way to wait is to destroy it.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <memory>

int main() {
    auto counter = std::make_shared<std::atomic<int>>(0);

    threadsafe::asynchronous_task_launcher launcher;
    for (int worker_index = 0; worker_index < 4; ++worker_index)
        launcher.launch_task(
            [](std::shared_ptr<std::atomic<int>> shared_counter) {
                for (int step = 0; step < 1000; ++step)
                    ++*shared_counter;
            },
            counter);

    std::printf("%d\n", counter->load());
}
