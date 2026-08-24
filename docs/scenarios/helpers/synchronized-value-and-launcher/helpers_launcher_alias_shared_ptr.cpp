#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

static_assert(threadsafe::launchable_task<
                  decltype([](std::shared_ptr<std::atomic<int>>) {}),
                  std::shared_ptr<std::atomic<int>>>);

int main() {
    threadsafe::asynchronous_task_launcher launcher;

    {
        std::atomic<int> stack_counter{0};
        // The aliasing constructor: owns the control block of `keep_alive`,
        // points at something it does not own at all.
        auto keep_alive = std::make_shared<int>(0);
        std::shared_ptr<std::atomic<int>> aliased(keep_alive, &stack_counter);

        launcher.launch_task(
            [](std::shared_ptr<std::atomic<int>> counter) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                std::printf("task wrote through the alias: %d\n",
                            counter->fetch_add(1) + 1);
            },
            aliased);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::printf("leaving the scope that owns stack_counter\n");
    }
    // launcher's destructor joins the task here, long after stack_counter died.
}
