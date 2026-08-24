// Same cancellable task, but the launcher now offers wait().
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <print>
#include <stop_token>

int main() {
    auto processed_items = std::make_shared<std::atomic<int>>(0);

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(
        [](std::stop_token token, std::shared_ptr<std::atomic<int>> counter) {
            for (int item = 0; item < 1000; ++item) {
                if (token.stop_requested())
                    return;
                counter->fetch_add(1, std::memory_order_relaxed);
            }
        },
        processed_items);
    launcher.wait();

    std::println("items processed out of 1000: {}", processed_items->load());
}
