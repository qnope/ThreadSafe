// Verifies the proposed synchronized_value<T, Mutex> opt-out: same safety,
// the caller picks the primitive.
#include <threadsafe/threadsafe.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

using automatic_form = threadsafe::synchronized_value<int>;
using explicit_form  = threadsafe::synchronized_value<int, std::mutex>;

static_assert(std::is_same_v<automatic_form::mutex, std::shared_mutex>,
              "default still deduces shared_mutex");
static_assert(std::is_same_v<explicit_form::mutex, std::mutex>,
              "explicit mutex is honoured");
static_assert(std::is_same_v<explicit_form::const_guard,
                             threadsafe::value_guard<const int, std::unique_lock<std::mutex>>>,
              "lock_shared() degrades to an exclusive lock, never to no lock");
static_assert(threadsafe::is_synchronizable_v<explicit_form>,
              "the trait specialisation still matches the two-parameter template");
static_assert(threadsafe::is_lifetime_aware_v<explicit_form>);
static_assert(!threadsafe::is_sendable_v<explicit_form::guard>);

namespace {
constexpr std::size_t per_thread = 100'000;
template <class Body>
double drive(unsigned thread_count, Body body) {
    std::atomic<bool> go{false};
    std::vector<std::jthread> workers;
    for (unsigned t = 0; t < thread_count; ++t)
        workers.emplace_back([&] {
            while (!go.load(std::memory_order_acquire)) {}
            for (std::size_t i = 0; i < per_thread; ++i) body();
        });
    auto begin = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_release);
    workers.clear();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(end - begin).count()
         / double(per_thread * thread_count);
}
}

int main() {
    std::printf("sizeof automatic (shared_mutex) = %zu, explicit std::mutex = %zu\n\n",
                sizeof(automatic_form), sizeof(explicit_form));
    std::printf("%8s %16s %18s %10s\n", "threads", "auto(shared)", "explicit(mutex)", "speedup");
    for (unsigned thread_count : {1u, 2u, 4u, 8u, 12u}) {
        automatic_form automatic_counter{0};
        explicit_form explicit_counter{0};
        double best_auto = 1e30, best_explicit = 1e30;
        for (int r = 0; r < 3; ++r) {
            double a = drive(thread_count, [&] { auto g = automatic_counter.lock(); *g += 1; });
            double e = drive(thread_count, [&] { auto g = explicit_counter.lock(); *g += 1; });
            best_auto = a < best_auto ? a : best_auto;
            best_explicit = e < best_explicit ? e : best_explicit;
        }
        std::printf("%8u %13.1f ns %15.1f ns %9.2fx\n",
                    thread_count, best_auto, best_explicit, best_auto / best_explicit);
    }
    explicit_form readable{7};
    auto guard = readable.lock_shared();
    std::printf("\nlock_shared() through the explicit std::mutex still reads: %d\n", *guard);
}
