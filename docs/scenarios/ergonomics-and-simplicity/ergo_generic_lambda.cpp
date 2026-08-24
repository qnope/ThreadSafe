// A generic lambda is the natural way to avoid spelling the argument type.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <print>

int main() {
    auto shared_counter = std::make_shared<std::atomic<int>>(0);
    {
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(
            [](auto counter) {
                for (int step = 0; step < 1000; ++step)
                    counter->fetch_add(1, std::memory_order_relaxed);
            },
            shared_counter);
    }
    std::println("counter = {}", shared_counter->load());
}
