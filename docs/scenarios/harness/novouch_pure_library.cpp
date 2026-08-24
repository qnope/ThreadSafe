// No THREADSAFE_UNSAFE_ASSERT_*, no raw new/delete, no const_cast, no cast at all.
// Only library types + std::reference_wrapper.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <thread>

using borrowed_counter = std::reference_wrapper<std::atomic<int>>;
using guarded_borrow = threadsafe::synchronized_value<borrowed_counter>;

static_assert(threadsafe::is_sendable_v<borrowed_counter>,
              "a reference_wrapper to an atomic is sendable");
static_assert(!threadsafe::is_lifetime_aware_v<borrowed_counter>,
              "but it does NOT keep the atomic alive");
static_assert(!threadsafe::is_lifetime_aware_v<guarded_borrow>,
              "synchronized_value propagates that correctly");
static_assert(!threadsafe::is_lifetime_aware_v<std::unique_ptr<guarded_borrow>>,
              "unique_ptr propagates it correctly");
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<guarded_borrow>>,
              "shared_ptr does NOT propagate it");
static_assert(threadsafe::launchable_task<
                  decltype([](std::shared_ptr<guarded_borrow>) {}),
                  std::shared_ptr<guarded_borrow>>,
              "launch_task accepts it");

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    {
        std::atomic<int> counter{0};
        auto shared = guarded_borrow::make(std::ref(counter));

        launcher.launch_task(
            [](std::shared_ptr<guarded_borrow> handle) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                auto locked = handle->lock();
                std::printf("task wrote: %d\n",
                            (*locked).get().fetch_add(1) + 1);
            },
            shared);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::printf("counter scope ends here\n");
    }
    std::printf("main falls off the end; launcher joins now\n");
}
