#include <threadsafe/threadsafe.h>

#include <functional>
#include <memory>
#include <string>

using SharedLog = threadsafe::synchronized_value<std::string>;

// 1. By value: impossible -- synchronized_value is neither copyable nor movable,
//    and Args... args is by value.
static_assert(!std::move_constructible<SharedLog>);
static_assert(!threadsafe::launchable_task<void(*)(SharedLog), SharedLog>);
static_assert(!threadsafe::launchable_scoped_task<void(*)(SharedLog), SharedLog>);

// 2. std::ref: sendable (synchronized_value is synchronizable) but never
//    lifetime aware, so only a SCOPED task takes it.
static_assert(threadsafe::is_sendable_v<std::reference_wrapper<SharedLog>>);
static_assert(!threadsafe::is_lifetime_aware_v<std::reference_wrapper<SharedLog>>);
static_assert(!threadsafe::launchable_task<void(*)(SharedLog&), std::reference_wrapper<SharedLog>>);
static_assert(threadsafe::launchable_scoped_task<void(*)(SharedLog&), std::reference_wrapper<SharedLog>>);

// 3. shared_ptr, via synchronized_value::make -- the only route into launch_task.
static_assert(threadsafe::launchable_task<void(*)(std::shared_ptr<SharedLog>),
                                          std::shared_ptr<SharedLog>>);

namespace {
// And the callable has to be a named type or a CAPTURELESS lambda, because a
// capturing closure is never sendable.
void append_line(std::shared_ptr<SharedLog> log, std::string line) {
    auto guard = log->lock();
    *guard += line;
}
}

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    auto log = SharedLog::make();
    launcher.launch_task(&append_line, log, std::string{"hello\n"});
    launcher.launch_task([](std::shared_ptr<SharedLog> shared_log) {
        auto guard = shared_log->lock();
        *guard += "world\n";
    }, log);
}
