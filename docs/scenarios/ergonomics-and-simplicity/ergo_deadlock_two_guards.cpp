// Two synchronized_values, two tasks, opposite orders. std::scoped_lock exists
// for exactly this, but value_guard takes its mutex in its constructor and never
// exposes it, so std::lock's deadlock-avoidance cannot be reached.
#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <print>
#include <thread>

using account = threadsafe::synchronized_value<int>;
using shared_account = std::shared_ptr<account>;

void transfer(const shared_account &from, const shared_account &to, int amount) {
    auto from_guard = from->lock();
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    auto to_guard = to->lock();
    *from_guard -= amount;
    *to_guard += amount;
}

int main() {
    auto first = account::make(100);
    auto second = account::make(100);

    std::thread watchdog{[] {
        std::this_thread::sleep_for(std::chrono::seconds{3});
        std::println("WATCHDOG: still blocked after 3 s — deadlocked");
        std::fflush(stdout);
        std::_Exit(42);
    }};
    watchdog.detach();

    {
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(
            [](shared_account from, shared_account to) { transfer(from, to, 30); },
            first, second);
        launcher.launch_task(
            [](shared_account from, shared_account to) { transfer(from, to, 30); },
            second, first);
    }

    auto first_guard = first->lock();
    auto second_guard = second->lock();
    std::println("no deadlock this run: {} {}", *first_guard, *second_guard);
}
