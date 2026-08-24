// The diagnostic says "specialize is_sendable to state the intent". Try to
// follow that advice for a lambda written where lambdas are written.
#include <threadsafe/threadsafe.h>

int main() {
    const int repeat_count = 10;
    auto task = [repeat_count] { return repeat_count; };

    template <>
    struct threadsafe::is_sendable<decltype(task)> : std::true_type {};

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(task);
}
