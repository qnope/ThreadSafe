// The shortest CORRECT hello-world: a counter shared across 4 threads.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <print>

int main() {
    auto counter = std::make_shared<std::atomic<int>>(0);

    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < 4; ++worker)
            launcher.launch_task(
                [](std::shared_ptr<std::atomic<int>> shared_counter) {
                    for (int step = 0; step < 1000; ++step)
                        shared_counter->fetch_add(1, std::memory_order_relaxed);
                },
                counter);
    }

    std::println("counter = {}", counter->load());
}
