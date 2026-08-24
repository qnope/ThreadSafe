#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

namespace {
// A user type that synchronizes itself, so the user vouches for it.
struct Session {
    std::atomic<int> hits{0};
};
// A plain struct holding a raw pointer -- the library knows this owns nothing.
struct SessionView {
    Session* borrowed;
};
}
// NO VOUCH

using guarded_view = threadsafe::synchronized_value<SessionView>;

static_assert(!threadsafe::is_lifetime_aware_v<SessionView>,
              "the library correctly refuses to call a raw-pointer struct owning");
static_assert(!threadsafe::is_lifetime_aware_v<guarded_view>,
              "and the wrapper propagates that correctly");
static_assert(!threadsafe::is_lifetime_aware_v<std::unique_ptr<guarded_view>>,
              "unique_ptr propagates it too");
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<guarded_view>>,
              "BUG: shared_ptr claims ownership unconditionally");
static_assert(threadsafe::launchable_task<
                  decltype([](std::shared_ptr<guarded_view>) {}),
                  std::shared_ptr<guarded_view>>,
              "BUG: launch_task therefore accepts the laundered borrow");

int main() {
    threadsafe::asynchronous_task_launcher launcher;

    {
        Session session;
        auto shared_view = guarded_view::make(SessionView{&session});

        launcher.launch_task(
            [](std::shared_ptr<guarded_view> view) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                auto locked = view->lock();
                std::printf("task touched the session: %d\n",
                            (*locked).borrowed->hits.fetch_add(1) + 1);
            },
            shared_view);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::printf("leaving the scope that owns the Session\n");
    }
}
