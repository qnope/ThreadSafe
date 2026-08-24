// Two synchronized_values, a transfer between them. std::scoped_lock cannot be
// used: value_guard is not Lockable and synchronized_value keeps its mutex
// private, so the only way to hold both is to take the guards one after the
// other -- which deadlocks the moment two transfers run in opposite directions.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <print>
#include <thread>

using account = threadsafe::synchronized_value<int>;

void transfer(account &from, account &to, int amount) {
    auto from_guard = from.lock();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto to_guard = to.lock();
    *from_guard -= amount;
    *to_guard += amount;
}

int main() {
    account first{100};
    account second{100};
    std::atomic<bool> finished{false};

    std::thread watchdog([&finished] {
        for (int tick = 0; tick < 20 && !finished.load(); ++tick)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!finished.load()) {
            std::println("DEADLOCK: both transfers are still waiting");
            std::fflush(nullptr);
            std::_Exit(0);
        }
    });

    {
        std::jthread left([&] { transfer(first, second, 10); });
        std::jthread right([&] { transfer(second, first, 10); });
    }

    finished.store(true);
    watchdog.join();
    const auto first_guard = first.lock();
    const auto second_guard = second.lock();
    std::println("transfers completed: {} / {}", *first_guard, *second_guard);
}
