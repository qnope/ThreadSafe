// (c) Producer / consumer -- the version the library accepts.
// std::queue is refused, so the buffer is a std::deque; value_guard never hands
// out its lock, so no std::condition_variable can be waited on -- the consumer
// polls.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <deque>
#include <memory>
#include <thread>

using PendingItems = threadsafe::synchronized_value<std::deque<int>>;

namespace {

struct ProducerConsumerChannel {
    std::shared_ptr<PendingItems> pending_items;
    std::shared_ptr<std::atomic<bool>> producer_finished;
};

}

int main() {
    const ProducerConsumerChannel channel{
        PendingItems::make(), std::make_shared<std::atomic<bool>>(false)};

    threadsafe::asynchronous_task_launcher launcher;

    launcher.launch_task(
        [](ProducerConsumerChannel producer_channel) {
            for (int item = 0; item < 10; ++item) {
                auto items = producer_channel.pending_items->lock();
                items->push_back(item);
            }
            producer_channel.producer_finished->store(true);
        },
        channel);

    launcher.launch_task(
        [](ProducerConsumerChannel consumer_channel) {
            int received_count = 0;
            while (true) {
                {
                    auto items = consumer_channel.pending_items->lock();
                    while (!items->empty()) {
                        std::printf("%d\n", items->front());
                        items->pop_front();
                        ++received_count;
                    }
                }
                if (consumer_channel.producer_finished->load()
                    && received_count == 10)
                    return;
                std::this_thread::yield();
            }
        },
        channel);
}
