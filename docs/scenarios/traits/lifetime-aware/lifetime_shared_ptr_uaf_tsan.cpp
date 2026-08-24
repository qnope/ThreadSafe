#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

struct CounterView {
    std::atomic<int> *target;
};

using SynchronizedView = threadsafe::synchronized_value<CounterView>;

static_assert(threadsafe::is_sendable_v<std::shared_ptr<SynchronizedView>>);
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<SynchronizedView>>);

void increment_later(std::shared_ptr<SynchronizedView> shared_view) {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    auto guard = shared_view->lock();
    guard->target->fetch_add(1, std::memory_order_relaxed);
    std::printf("worker wrote through the freed borrow\n");
}

int main() {
    threadsafe::asynchronous_task_launcher launcher;

    std::atomic<int> *counter = new std::atomic<int>{0};
    auto shared_view = SynchronizedView::make(CounterView{counter});

    launcher.launch_task(&increment_later, shared_view);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    delete counter;

    std::vector<int *> reused;
    for (int index = 0; index < 4096; ++index)
        reused.push_back(new int(index));

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for (int *block : reused)
        delete block;
    std::printf("main done\n");
    return 0;
}
