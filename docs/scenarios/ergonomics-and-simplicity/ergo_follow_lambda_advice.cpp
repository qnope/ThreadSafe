// Following the diagnostic's advice literally: "specialize is_sendable to state
// the intent" — for a closure type created inside main().
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>

int main() {
    auto shared_counter = std::make_shared<std::atomic<int>>(0);
    auto task = [shared_counter] { shared_counter->fetch_add(1); };

    template <>
    struct threadsafe::is_sendable<decltype(task)> : std::true_type {};

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(task);
}
