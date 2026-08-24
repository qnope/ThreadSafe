// Producer / consumer with only what the library offers: no condition variable
// can be attached to a value_guard, so the consumer has to spin.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <deque>
#include <memory>
#include <optional>
#include <print>
#include <thread>

using queue_type = threadsafe::synchronized_value<std::deque<int>>;

int main() {
    auto shared_queue = queue_type::make();
    auto producer_finished = std::make_shared<std::atomic<bool>>(false);
    auto consumed_total = std::make_shared<std::atomic<int>>(0);

    {
        threadsafe::asynchronous_task_launcher launcher;

        launcher.launch_task(
            [](std::shared_ptr<queue_type> queue,
               std::shared_ptr<std::atomic<bool>> finished) {
                for (int item = 0; item < 1000; ++item) {
                    auto guard = queue->lock();
                    guard->push_back(item);
                }
                finished->store(true);
            },
            shared_queue, producer_finished);

        launcher.launch_task(
            [](std::shared_ptr<queue_type> queue,
               std::shared_ptr<std::atomic<bool>> finished,
               std::shared_ptr<std::atomic<int>> total) {
                while (true) {
                    std::optional<int> item;
                    {
                        auto guard = queue->lock();
                        if (!guard->empty()) {
                            item = guard->front();
                            guard->pop_front();
                        }
                    }
                    if (item)
                        total->fetch_add(*item, std::memory_order_relaxed);
                    else if (finished->load())
                        return;
                    else
                        std::this_thread::yield();
                }
            },
            shared_queue, producer_finished, consumed_total);
    }

    std::println("consumed total = {} (expected {})", consumed_total->load(),
                 999 * 1000 / 2);
}
