#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <thread>

using borrowed_counter = std::reference_wrapper<std::atomic<int>>;
using guarded_borrow = threadsafe::synchronized_value<borrowed_counter>;

// Every check the library offers passes.
static_assert(threadsafe::is_sendable_v<guarded_borrow>);
static_assert(threadsafe::is_synchronizable_v<guarded_borrow>);
static_assert(!threadsafe::is_lifetime_aware_v<guarded_borrow>,
              "the library correctly knows the wrapper owns nothing");
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<guarded_borrow>>,
              "yet a shared_ptr to it claims ownership -- transitivity broken");
static_assert(threadsafe::launchable_task<
                  decltype([](std::shared_ptr<guarded_borrow>) {}),
                  std::shared_ptr<guarded_borrow>>,
              "launch_task accepts the laundered borrow");

int main() {
    threadsafe::asynchronous_task_launcher launcher;

    auto* counter = new std::atomic<int>{0};
    auto shared_borrow = guarded_borrow::make(std::ref(*counter));

    launcher.launch_task(
        [](std::shared_ptr<guarded_borrow> guarded) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            auto locked = guarded->lock();
            const int seen = (*locked).get().fetch_add(1) + 1;
            std::printf("task read the borrowed counter: %d\n", seen);
        },
        shared_borrow);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::printf("main frees the referent while the task still holds the borrow\n");
    delete counter;
}
