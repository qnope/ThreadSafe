// Two synchronized_values locked in opposite orders. The API offers no way to
// take both locks at once, so any two-account transfer must nest lock() calls,
// and nesting is where the deadlock lives.
//
// A watchdog thread aborts after 2 seconds so the test terminates.
//
// g++-16 -std=c++26 -freflection -I<include> -g -O1 -pthread deadlock.cpp -o deadlock && ./deadlock
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

using account_type = threadsafe::synchronized_value<int>;

void transfer(account_type& from_account, account_type& to_account, int amount) {
    const auto from_guard = from_account.lock();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const auto to_guard = to_account.lock();          // nested: order matters
    *from_guard -= amount;
    *to_guard += amount;
}

int main() {
    account_type alice{100};
    account_type bob{100};
    std::atomic<bool> finished{false};

    std::thread watchdog{[&] {
        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline)
            if (finished.load()) {
                std::printf("no deadlock this run\n");
                return;
            }
        std::printf("DEADLOCK: both transfers still blocked after 2s\n");
        std::fflush(stdout);
        std::_Exit(3);
    }};
    watchdog.detach();

    std::thread first{[&] { transfer(alice, bob, 10); }};
    std::thread second{[&] { transfer(bob, alice, 10); }};

    first.join();
    second.join();
    finished.store(true);

    std::printf("completed, alice=? bob=?\n");
    return 0;
}
