// The diagnostic for a capturing lambda says "specialize is_sendable to state
// the intent". Try to follow that advice for the lambda the user actually
// wrote, at the call site.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>

int main() {
    auto counter = std::make_shared<std::atomic<int>>(0);
    auto increment_counter = [counter] { ++*counter; };

    template <>
    struct threadsafe::is_sendable<decltype(increment_counter)>
        : std::true_type {};

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(increment_counter);
}
