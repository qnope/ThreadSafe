#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <memory>
#include <stop_token>

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

// The library says: a stop_source may be used from several threads at once.
static_assert(is_synchronizable_v<std::stop_source>);
// Therefore a mutable reference / pointer to one may be shared.
static_assert(is_sendable_v<std::stop_source&>);
static_assert(is_sendable_v<std::stop_source*>);
// And a shared_ptr to one is both sendable and lifetime-aware, so the launcher
// itself hands the very same mutable stop_source object to two threads.
static_assert(is_sendable_v<std::shared_ptr<std::stop_source>>);
static_assert(is_lifetime_aware_v<std::shared_ptr<std::stop_source>>);
static_assert(threadsafe::launchable_task<
                  void (*)(std::shared_ptr<std::stop_source>),
                  std::shared_ptr<std::stop_source>>);

void hammer(std::shared_ptr<std::stop_source> shared_source) {
    for (int iteration = 0; iteration < 20000; ++iteration) {
        std::stop_source fresh_source;
        *shared_source = fresh_source;      // operator= is NOT race-free
    }
}

int main() {
    auto shared_source = std::make_shared<std::stop_source>();

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(&hammer, shared_source);
    launcher.launch_task(&hammer, shared_source);

    std::printf("done, stop_possible=%d\n", int(shared_source->stop_possible()));
}
