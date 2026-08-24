// The launcher offers no way to wait for its tasks: the only join point is its
// destructor, and ~jthread requests a stop before joining. A cancellable task
// is therefore cancelled at the exact moment the user asks to wait for it.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <print>
#include <stop_token>

int main() {
    auto processed_items = std::make_shared<std::atomic<int>>(0);

    {
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(
            [](std::stop_token token,
               std::shared_ptr<std::atomic<int>> counter) {
                for (int item = 0; item < 1000; ++item) {
                    if (token.stop_requested())
                        return;
                    counter->fetch_add(1, std::memory_order_relaxed);
                }
            },
            processed_items);
        // The user has no launcher.wait() to call here.
    }

    std::println("items processed out of 1000: {}", processed_items->load());
}
