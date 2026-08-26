#include <threadsafe/threadsafe.h>
#include <string>

// Namespace-scope closure: this is the ONLY closure a user can name for a
// specialization, because a specialization must be written at namespace scope.
namespace {
std::string message_prototype;
auto safe_task = [m = std::string{"hello"}] { (void)m.size(); };
using SafeTask = decltype(safe_task);
}

template <> struct threadsafe::is_sendable<SafeTask>       : std::true_type {};
template <> struct threadsafe::is_lifetime_aware<SafeTask> : std::true_type {};

static_assert(threadsafe::launchable_task<SafeTask>,
              "a namespace-scope closure can be blessed");

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(safe_task);

    // But a closure created inside a function cannot be blessed at all:
    // std::string local = "x";
    // auto inner = [local] {};
    // template <> struct threadsafe::is_sendable<decltype(inner)> ... // ill-formed:
    //   a template specialization must appear at namespace scope.
}
