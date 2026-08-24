#include <threadsafe/threadsafe.h>

#include "perf_bench.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace probe {

std::atomic<std::uint64_t> sink{0};

// A large trivially copyable argument: moving it is a memcpy of the whole
// array, so an extra move is measurable.
template <std::size_t Bytes>
struct bulky_payload {
    std::array<unsigned char, Bytes> storage{};
};

struct move_counting_payload {
    static inline std::atomic<std::uint64_t> copies{0};
    static inline std::atomic<std::uint64_t> moves{0};

    std::array<unsigned char, 4096> storage{};

    move_counting_payload() = default;
    move_counting_payload(const move_counting_payload& other)
        : storage(other.storage) { copies.fetch_add(1, std::memory_order_relaxed); }
    move_counting_payload(move_counting_payload&& other) noexcept
        : storage(other.storage) { moves.fetch_add(1, std::memory_order_relaxed); }
    move_counting_payload& operator=(const move_counting_payload&) = default;
    move_counting_payload& operator=(move_counting_payload&&) = default;
    ~move_counting_payload() = default;
};

struct empty_task {
    void operator()() const { sink.fetch_add(1, std::memory_order_relaxed); }
};

struct counting_task {
    void operator()(move_counting_payload payload) const {
        sink.fetch_add(payload.storage[0] + 1u, std::memory_order_relaxed);
    }
};

using big_payload = bulky_payload<1u << 20>;   // 1 MiB

struct big_task {
    void operator()(big_payload payload) const {
        sink.fetch_add(payload.storage[0] + 1u, std::memory_order_relaxed);
    }
};

}

// The counters make the copy/move user-written, which the structural rule
// rejects on purpose. Vouch for it: the payload is a plain byte array.
template <>
struct threadsafe::is_sendable<probe::move_counting_payload> : std::true_type {};
template <>
struct threadsafe::is_lifetime_aware<probe::move_counting_payload> : std::true_type {};

namespace {

constexpr std::uint64_t thread_launches = 2000;
constexpr std::uint64_t big_launches = 200;

}

int main() {
    using namespace probe;
    static_assert(threadsafe::launchable_task<counting_task, move_counting_payload>);
    static_assert(threadsafe::launchable_task<big_task, big_payload>);

    // ---- 1. thread launch cost, no argument -------------------------------
    bench::report("launch_task(f)  [empty callable]",
        bench::measure(1, 5, thread_launches, [] {
            threadsafe::asynchronous_task_launcher launcher;
            for (std::uint64_t index = 0; index < thread_launches; ++index)
                launcher.launch_task(empty_task{});
        }));
    bench::report("vector<jthread>.emplace_back(f)",
        bench::measure(1, 5, thread_launches, [] {
            std::vector<std::jthread> threads;
            for (std::uint64_t index = 0; index < thread_launches; ++index)
                threads.emplace_back(empty_task{});
        }));
    bench::report("vector<jthread> RESERVED .emplace_back(f)",
        bench::measure(1, 5, thread_launches, [] {
            std::vector<std::jthread> threads;
            threads.reserve(thread_launches);
            for (std::uint64_t index = 0; index < thread_launches; ++index)
                threads.emplace_back(empty_task{});
        }));
    bench::report("std::jthread{f} joined immediately",
        bench::measure(1, 5, thread_launches, [] {
            for (std::uint64_t index = 0; index < thread_launches; ++index)
                std::jthread thread{empty_task{}};
        }));
    std::printf("\n");

    // ---- 2. how many copies/moves does each spelling cost? ----------------
    auto report_counts = [](const char* label, auto action) {
        move_counting_payload::copies.store(0);
        move_counting_payload::moves.store(0);
        action();
        std::printf("%-52s copies=%llu moves=%llu (bytes touched = %llu KiB)\n",
                    label,
                    (unsigned long long)move_counting_payload::copies.load(),
                    (unsigned long long)move_counting_payload::moves.load(),
                    (unsigned long long)((move_counting_payload::copies.load()
                                        + move_counting_payload::moves.load()) * 4));
    };

    report_counts("launch_task(f, std::move(arg))", [] {
        threadsafe::asynchronous_task_launcher launcher;
        move_counting_payload payload;
        launcher.launch_task(counting_task{}, std::move(payload));
    });
    report_counts("vector<jthread>.emplace_back(f, std::move(arg))", [] {
        std::vector<std::jthread> threads;
        move_counting_payload payload;
        threads.emplace_back(counting_task{}, std::move(payload));
    });
    report_counts("launch_task(f, lvalue arg)", [] {
        threadsafe::asynchronous_task_launcher launcher;
        move_counting_payload payload;
        launcher.launch_task(counting_task{}, payload);
    });
    report_counts("vector<jthread>.emplace_back(f, lvalue arg)", [] {
        std::vector<std::jthread> threads;
        move_counting_payload payload;
        threads.emplace_back(counting_task{}, payload);
    });
    report_counts("launch_task(f, prvalue arg)", [] {
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(counting_task{}, move_counting_payload{});
    });
    report_counts("vector<jthread>.emplace_back(f, prvalue arg)", [] {
        std::vector<std::jthread> threads;
        threads.emplace_back(counting_task{}, move_counting_payload{});
    });
    std::printf("\n");

    // ---- 3. the wall-clock price of that extra move for a big argument ----
    bench::report("1 MiB arg: launch_task(f, std::move(arg))",
        bench::measure(1, 5, big_launches, [] {
            threadsafe::asynchronous_task_launcher launcher;
            for (std::uint64_t index = 0; index < big_launches; ++index) {
                big_payload payload;
                launcher.launch_task(big_task{}, std::move(payload));
            }
        }));
    bench::report("1 MiB arg: emplace_back(f, std::move(arg))",
        bench::measure(1, 5, big_launches, [] {
            std::vector<std::jthread> threads;
            for (std::uint64_t index = 0; index < big_launches; ++index) {
                big_payload payload;
                threads.emplace_back(big_task{}, std::move(payload));
            }
        }));
    std::printf("\n");

    // ---- 4. launch_scoped_task: a whole thread for a synchronous call -----
    bench::report("launch_scoped_task(f) [spawn + join]",
        bench::measure(1, 5, thread_launches, [] {
            threadsafe::asynchronous_task_launcher launcher;
            for (std::uint64_t index = 0; index < thread_launches; ++index)
                launcher.launch_scoped_task(empty_task{});
        }));
    bench::report("direct call f() (no thread at all)",
        bench::measure(1, 5, thread_launches, [] {
            empty_task callable;
            for (std::uint64_t index = 0; index < thread_launches; ++index)
                callable();
        }));

    std::printf("\nsink=%llu\n", (unsigned long long)sink.load());
}
