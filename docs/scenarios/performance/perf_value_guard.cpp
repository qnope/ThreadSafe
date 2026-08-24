#include <threadsafe/threadsafe.h>

#include "perf_bench.h"

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <shared_mutex>

namespace {

struct payload {
    int counters[8];
};

using library_value = threadsafe::synchronized_value<payload>;

struct hand_rolled {
    mutable std::shared_mutex mutex;
    payload value{};
};

std::uint64_t sink = 0;

constexpr std::uint64_t iterations = 20000000;

}

int main() {
    std::printf("sizeof(std::mutex)                              = %zu\n", sizeof(std::mutex));
    std::printf("sizeof(std::shared_mutex)                       = %zu\n", sizeof(std::shared_mutex));
    std::printf("sizeof(std::lock_guard<std::mutex>)             = %zu\n", sizeof(std::lock_guard<std::mutex>));
    std::printf("sizeof(std::unique_lock<std::shared_mutex>)     = %zu\n", sizeof(std::unique_lock<std::shared_mutex>));
    std::printf("sizeof(std::shared_lock<std::shared_mutex>)     = %zu\n", sizeof(std::shared_lock<std::shared_mutex>));
    std::printf("sizeof(library_value::guard)                    = %zu\n", sizeof(library_value::guard));
    std::printf("sizeof(library_value::const_guard)              = %zu\n", sizeof(library_value::const_guard));
    std::printf("sizeof(synchronized_value<payload>)             = %zu\n", sizeof(library_value));
    std::printf("sizeof(hand_rolled shared_mutex + payload)      = %zu\n", sizeof(hand_rolled));
    std::printf("\n");

    library_value library{};
    hand_rolled hand{};

    const auto library_write = bench::measure(2, 9, iterations, [&] {
        std::uint64_t local = 0;
        for (std::uint64_t index = 0; index < iterations; ++index) {
            auto held = library.lock();
            held->counters[index & 7] += 1;
            local += std::uint64_t(held->counters[0]);
        }
        sink += local;
    });
    const auto hand_write = bench::measure(2, 9, iterations, [&] {
        std::uint64_t local = 0;
        for (std::uint64_t index = 0; index < iterations; ++index) {
            std::lock_guard<std::shared_mutex> held{hand.mutex};
            hand.value.counters[index & 7] += 1;
            local += std::uint64_t(hand.value.counters[0]);
        }
        sink += local;
    });
    const auto library_read = bench::measure(2, 9, iterations, [&] {
        std::uint64_t local = 0;
        const library_value& read_only = library;
        for (std::uint64_t index = 0; index < iterations; ++index) {
            auto held = read_only.lock_shared();
            local += std::uint64_t(held->counters[index & 7]);
        }
        sink += local;
    });
    const auto hand_read = bench::measure(2, 9, iterations, [&] {
        std::uint64_t local = 0;
        for (std::uint64_t index = 0; index < iterations; ++index) {
            std::shared_lock<std::shared_mutex> held{hand.mutex};
            local += std::uint64_t(hand.value.counters[index & 7]);
        }
        sink += local;
    });

    bench::report("synchronized_value::lock()   (exclusive)", library_write);
    bench::report("hand lock_guard<shared_mutex> (exclusive)", hand_write);
    bench::report("synchronized_value::lock_shared()", library_read);
    bench::report("hand shared_lock<shared_mutex>", hand_read);
    std::printf("\nsink=%llu\n", (unsigned long long)sink);
}
