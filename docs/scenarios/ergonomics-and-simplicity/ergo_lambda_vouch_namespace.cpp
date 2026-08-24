// The only shape the advice can take: hoist the lambda to namespace scope and
// specialize on its closure type.
#include <threadsafe/threadsafe.h>

#include <print>

namespace app {
inline constexpr int repeat_count = 3;
inline auto task = [] { std::println("running {} times", repeat_count); };
}

template <>
struct threadsafe::is_sendable<decltype(app::task)> : std::true_type {};
template <>
struct threadsafe::is_lifetime_aware<decltype(app::task)> : std::true_type {};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(app::task);
}
