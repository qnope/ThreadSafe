// launch_scoped_task joins before returning, so a const reference to a stack
// value cannot dangle. is_synchronizable_v<const int> is true. The call is
// still refused.
#include <threadsafe/threadsafe.h>

#include <functional>
#include <print>
#include <string>

int main() {
    const std::string banner = "hello";
    const int repetitions = 3;

    threadsafe::asynchronous_task_launcher launcher;
    static_assert(threadsafe::is_synchronizable_v<const int>);
    launcher.launch_scoped_task(
        [](const int &count) { std::println("{}", count); },
        std::cref(repetitions));
}
