// The proposed API, exercised: `with` makes the check-then-act atomic, `apply`
// locks two synchronized_values at once without an ordering rule, and the same
// transfer that deadlocked with nested lock() calls now completes.
//
// g++-16 -std=c++26 -freflection -I<include-patched> -g -O1 -pthread patched_api.cpp -o api && ./api
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

using registry_type = threadsafe::synchronized_value<std::vector<std::string>>;
using account_type = threadsafe::synchronized_value<int>;

int main() {
    // 1. check-then-act, now one critical section.
    constexpr int thread_count = 8;
    int runs_with_duplicates = 0;

    for (int attempt = 0; attempt < 2000; ++attempt) {
        registry_type registry{};
        std::atomic<int> ready{0};
        {
            std::vector<std::jthread> workers;
            for (int index = 0; index < thread_count; ++index)
                workers.emplace_back([&] {
                    ready.fetch_add(1);
                    while (ready.load() < thread_count) {}
                    registry.with([](std::vector<std::string>& endpoints) {
                        if (endpoints.empty())
                            endpoints.push_back("default-endpoint");
                    });
                });
        }
        if (registry.with_shared(
                [](const std::vector<std::string>& endpoints) {
                    return endpoints.size();
                })
            != 1u)
            ++runs_with_duplicates;
    }
    std::printf("with(): duplicate registrations in 2000 runs = %d\n",
                runs_with_duplicates);

    // 2. two accounts, opposite orders, no deadlock.
    account_type alice{100};
    account_type bob{100};
    std::atomic<bool> finished{false};

    std::thread watchdog{[&] {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline)
            if (finished.load())
                return;
        std::printf("DEADLOCK with with_all()\n");
        std::fflush(stdout);
        std::_Exit(3);
    }};
    watchdog.detach();

    const auto transfer = [](account_type& from_account,
                             account_type& to_account, int amount) {
        for (int repetition = 0; repetition < 20000; ++repetition)
            threadsafe::with_all(
                [amount](int& from_balance, int& to_balance) {
                    from_balance -= amount;
                    to_balance += amount;
                },
                from_account, to_account);
    };

    {
        std::jthread first{[&] { transfer(alice, bob, 10); }};
        std::jthread second{[&] { transfer(bob, alice, 10); }};
    }
    finished.store(true);

    const int total = threadsafe::with_all_shared(
        [](const int& first_balance, const int& second_balance) {
            return first_balance + second_balance;
        },
        alice, bob);
    std::printf("with_all(): no deadlock, conserved total = %d (expected 200)\n",
                total);
    return 0;
}
