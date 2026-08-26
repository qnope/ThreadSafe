// A realistic check-then-act bug that the current API not only permits but
// pushes you towards: the deleted rvalue operator* forbids
//     if (registry.lock()->empty()) registry.lock()->push_back(...);
// so the user splits it into two guards -- and the split is exactly the bug.
//
// g++-16 -std=c++26 -freflection -I<include> -g -O1 -pthread check_then_act.cpp -o cta && ./cta
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

using registry_type = threadsafe::synchronized_value<std::vector<std::string>>;

// "Register the default endpoint once, if nobody registered anything yet."
void register_default_once(registry_type& registry, std::atomic<int>& ready,
                           int thread_count) {
    ready.fetch_add(1);
    while (ready.load() < thread_count) {}

    bool is_empty = false;
    {
        const auto reader_guard = registry.lock_shared();   // lock #1: CHECK
        is_empty = reader_guard->empty();
    }                                                       // lock released

    if (is_empty) {
        const auto writer_guard = registry.lock();          // lock #2: ACT
        writer_guard->push_back("default-endpoint");
    }
}

int main() {
    constexpr int thread_count = 8;
    int runs_with_duplicates = 0;

    for (int attempt = 0; attempt < 2000; ++attempt) {
        registry_type registry{};
        std::atomic<int> ready{0};

        {
            std::vector<std::jthread> workers;
            for (int index = 0; index < thread_count; ++index)
                workers.emplace_back([&] {
                    register_default_once(registry, ready, thread_count);
                });
        }

        const auto final_guard = registry.lock_shared();
        if (final_guard->size() != 1)
            ++runs_with_duplicates;
    }

    std::printf("runs where the \"once\" registration happened more than once: "
                "%d / 2000\n", runs_with_duplicates);
    return 0;
}
