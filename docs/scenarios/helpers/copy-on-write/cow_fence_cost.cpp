// Cost of the unique-branch acquire on the as_mutable fast path:
//   (a) std::atomic_thread_fence(acquire)  -- what the library does
//   (b) a throw-away shared_ptr copy       -- an acq_rel RMW pair, TSan-visible
//   (c) nothing at all                     -- the unsound baseline
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
struct Config {
    std::vector<int> table = std::vector<int>(256, 1);
    std::string name = "configuration-object-with-a-heap-allocated-name";
};

enum class Acquire { fence, rmw, none };

template <Acquire acquire_kind>
struct Cow {
    std::shared_ptr<Config> ptr_ = std::make_shared<Config>();
    Config& as_mutable() {
        if (ptr_.use_count() != 1) {
            ptr_ = std::make_shared<Config>(std::as_const(*ptr_));
        } else if constexpr (acquire_kind == Acquire::fence) {
            std::atomic_thread_fence(std::memory_order_acquire);
        } else if constexpr (acquire_kind == Acquire::rmw) {
            [[maybe_unused]] const std::shared_ptr<Config> acquiring = ptr_;
        }
        return *ptr_;
    }
};

constexpr int iterations = 20'000'000;
volatile long long sink = 0;

template <Acquire acquire_kind>
double measure(const char* label) {
    Cow<acquire_kind> cow;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration != iterations; ++iteration)
        cow.as_mutable().table[iteration & 255] = iteration;
    const auto finished = std::chrono::steady_clock::now();
    sink += cow.as_mutable().table[0];
    const double ms = std::chrono::duration<double, std::milli>(finished - started).count();
    std::printf("%-28s %8.1f ms  (%5.2f ns / as_mutable)\n", label, ms,
                ms * 1e6 / iterations);
    return ms;
}
}

int main() {
    // make sure the process is genuinely multi-threaded, so libstdc++ does not
    // take its single-threaded refcount shortcut
    std::thread idle([] { std::this_thread::yield(); });
    idle.join();

    for (int repetition = 0; repetition != 3; ++repetition) {
        measure<Acquire::none>("no acquire (unsound)");
        measure<Acquire::fence>("atomic_thread_fence(acquire)");
        measure<Acquire::rmw>("shared_ptr copy (acq_rel RMW)");
        std::printf("--\n");
    }
    return 0;
}
