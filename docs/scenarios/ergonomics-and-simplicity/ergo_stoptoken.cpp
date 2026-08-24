// jthread injects a stop_token; does launch_task let a cancellable task use it?
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <print>
#include <stop_token>

int main() {
    auto ticks = std::make_shared<std::atomic<int>>(0);
    {
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(
            [](std::stop_token token, std::shared_ptr<std::atomic<int>> counter) {
                while (!token.stop_requested())
                    counter->fetch_add(1, std::memory_order_relaxed);
            },
            ticks);
    }
    std::println("ticks = {}", ticks->load() > 0);
}
