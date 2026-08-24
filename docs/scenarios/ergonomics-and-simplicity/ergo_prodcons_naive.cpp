// (c) Producer / consumer -- the naive version.
#include <threadsafe/threadsafe.h>

#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <queue>

int main() {
    std::queue<int> pending_items;
    std::mutex pending_items_mutex;
    std::condition_variable item_available;
    bool producer_finished = false;

    threadsafe::asynchronous_task_launcher launcher;

    launcher.launch_task([&] {
        for (int item = 0; item < 10; ++item) {
            std::lock_guard lock{pending_items_mutex};
            pending_items.push(item);
            item_available.notify_one();
        }
        std::lock_guard lock{pending_items_mutex};
        producer_finished = true;
        item_available.notify_all();
    });

    launcher.launch_task([&] {
        while (true) {
            std::unique_lock lock{pending_items_mutex};
            item_available.wait(lock, [&] {
                return !pending_items.empty() || producer_finished;
            });
            if (pending_items.empty())
                return;
            std::printf("%d\n", pending_items.front());
            pending_items.pop();
        }
    });
}
