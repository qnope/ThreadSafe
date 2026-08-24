// The smallest counter-sharing program the library actually accepts.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <print>

int main() {
    auto shared_counter = std::make_shared<std::atomic<int>>(0);

    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int task_index = 0; task_index < 4; ++task_index)
            launcher.launch_task(
                [](std::shared_ptr<std::atomic<int>> counter) {
                    for (int step = 0; step < 100000; ++step)
                        counter->fetch_add(1, std::memory_order_relaxed);
                },
                shared_counter);
    }

    std::println("counter = {}", shared_counter->load());
}
