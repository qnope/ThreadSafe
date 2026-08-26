// The trait blesses `const HitCounter`: reflection sees one int, and one int is
// read-safe. But next() is const and mutates a function-local static, which the
// structural walk cannot see. synchronized_value therefore picks a
// std::shared_mutex and lock_shared() lets every reader in at once.
//
// g++-16 -std=c++26 -freflection -I<include> -g -O1 -pthread race_static_gcc.cpp -o race && ./race
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <concepts>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

struct HitCounter {
    int seed;
    int next() const {
        static int calls = 0;
        return seed + (++calls);   // written under a *shared* lock
    }
};

static_assert(threadsafe::is_sendable_v<HitCounter>);
static_assert(threadsafe::is_synchronizable_v<const HitCounter>,
              "the trait says a const HitCounter is safe to read concurrently");
static_assert(std::same_as<threadsafe::synchronized_value<HitCounter>::mutex,
                           std::mutex>, "patched: exclusive by default");
static_assert(std::same_as<
    threadsafe::synchronized_value<HitCounter>::const_guard,
    threadsafe::value_guard<const HitCounter,
                            std::unique_lock<std::mutex>>>);

int main() {
    constexpr int reader_count = 8;
    constexpr int calls_per_reader = 20000;

    threadsafe::synchronized_value<HitCounter> shared_counter{HitCounter{0}};

    std::atomic<int> ready{0};
    int highest_seen = 0;
    std::mutex report_mutex;

    {
        std::vector<std::jthread> readers;
        for (int index = 0; index < reader_count; ++index)
            readers.emplace_back([&] {
                ready.fetch_add(1);
                while (ready.load() < reader_count) {}
                int local_highest = 0;
                for (int call = 0; call < calls_per_reader; ++call) {
                    const auto reader_guard = shared_counter.lock_shared();
                    local_highest = std::max(local_highest, reader_guard->next());
                }
                const std::lock_guard<std::mutex> report{report_mutex};
                highest_seen = std::max(highest_seen, local_highest);
            });
    }

    const int expected = reader_count * calls_per_reader;
    std::printf("expected highest ticket: %d\n", expected);
    std::printf("actual   highest ticket: %d\n", highest_seen);
    std::printf("increments lost to the race: %d\n", expected - highest_seen);
    return 0;
}
