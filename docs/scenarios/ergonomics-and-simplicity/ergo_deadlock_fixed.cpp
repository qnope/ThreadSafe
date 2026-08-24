// The same two transfers in opposite orders, taking both locks at once.
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
    from->lock_both(*to, [amount](int &from_balance, int &to_balance) {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        from_balance -= amount;
        to_balance += amount;
    });
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
    std::println("no deadlock: {} {}", *first_guard, *second_guard);
}
