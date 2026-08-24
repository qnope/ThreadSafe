#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

struct Counter {
    std::atomic<int> hits{0};
};

using WeakCounter = std::weak_ptr<std::atomic<int>>;

static_assert(threadsafe::is_sendable_v<WeakCounter>);
static_assert(threadsafe::is_lifetime_aware_v<WeakCounter>);
static_assert(threadsafe::launchable_task<void (*)(WeakCounter), WeakCounter>,
              "launch_task accepts a weak_ptr");

void touch_later(WeakCounter weak_counter) {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (auto locked = weak_counter.lock()) {
        locked->fetch_add(1, std::memory_order_relaxed);
        std::printf("worker: locked, value now %d\n",
                    locked->load(std::memory_order_relaxed));
    } else {
        std::printf("worker: lock() returned null, nothing dereferenced\n");
    }
}

int main() {
    threadsafe::asynchronous_task_launcher launcher;

    WeakCounter weak_counter;
    {
        auto owner = std::make_shared<std::atomic<int>>(0);
        weak_counter = owner;
        launcher.launch_task(&touch_later, weak_counter);
    }
    std::printf("main: the shared_ptr owner is gone, object destroyed\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    return 0;
}
