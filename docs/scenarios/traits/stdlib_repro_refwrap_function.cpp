#include <threadsafe/threadsafe.h>

#include <functional>

void free_function() {}

// A raw function pointer is lifetime aware: functions have static storage
// duration, so nothing can dangle. lifetime_aware.h says so explicitly.
static_assert(threadsafe::is_lifetime_aware_v<void (*)()>);

// std::reference_wrapper<void()> is the same referent through the same kind of
// indirection -- and it is refused.
static_assert(!threadsafe::is_lifetime_aware_v<std::reference_wrapper<void()>>,
              "OBSERVED: the library says a reference to a function borrows");

// The consequence: std::ref of a function cannot be handed to launch_task.
void run(std::reference_wrapper<void()> callback) { callback(); }

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(&run, std::ref(free_function));
}
