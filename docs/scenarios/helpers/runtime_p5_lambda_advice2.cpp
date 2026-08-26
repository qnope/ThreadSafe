#include <threadsafe/threadsafe.h>
#include <memory>
using counter = threadsafe::synchronized_value<int>;
int main() {
    auto total = counter::make(0);
    threadsafe::asynchronous_task_launcher launcher;
    auto task = [total] { auto guard = total->lock(); *guard += 1; };
    template <> struct threadsafe::is_sendable<decltype(task)> : std::true_type {};
    launcher.launch_task(task);
}
