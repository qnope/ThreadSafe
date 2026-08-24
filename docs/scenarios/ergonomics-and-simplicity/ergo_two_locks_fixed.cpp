// The same transfer, with the proposed with_all_locked helper.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <print>
#include <thread>

using account = threadsafe::synchronized_value<int>;

void transfer(account &from, account &to, int amount) {
    threadsafe::with_all_locked(
        [amount](int &from_value, int &to_value) {
            from_value -= amount;
            to_value += amount;
        },
        from, to);
}

int main() {
    account first{100};
    account second{100};

    {
        std::jthread left([&] {
            for (int round = 0; round < 10000; ++round)
                transfer(first, second, 10);
        });
        std::jthread right([&] {
            for (int round = 0; round < 10000; ++round)
                transfer(second, first, 10);
        });
    }

    const auto first_guard = first.lock();
    const auto second_guard = second.lock();
    std::println("balances: {} / {}", *first_guard, *second_guard);
}
