#include <threadsafe/threadsafe.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

using clock_type = std::chrono::steady_clock;

template <class Body>
double best_nanoseconds_per_iteration(int rounds, int iterations, Body body) {
    double best = 1e30;
    for (int round = 0; round < rounds; ++round) {
        const auto start = clock_type::now();
        for (int step = 0; step < iterations; ++step) body();
        const auto elapsed = clock_type::now() - start;
        best = std::min(best,
                        std::chrono::duration<double, std::nano>(elapsed).count()
                            / iterations);
    }
    return best;
}

int main() {
    constexpr int rounds = 7;
    constexpr int lock_iterations = 2'000'000;

    std::mutex plain_mutex;
    int plain_value = 0;
    const double plain = best_nanoseconds_per_iteration(rounds, lock_iterations, [&] {
        std::lock_guard<std::mutex> held(plain_mutex);
        plain_value += 1;
    });

    std::shared_mutex plain_shared_mutex;
    int shared_value = 0;
    const double shared_exclusive =
        best_nanoseconds_per_iteration(rounds, lock_iterations, [&] {
            std::unique_lock<std::shared_mutex> held(plain_shared_mutex);
            shared_value += 1;
        });

    threadsafe::synchronized_value<int> guarded_value{0};
    const double guarded =
        best_nanoseconds_per_iteration(rounds, lock_iterations, [&] {
            auto held = guarded_value.lock();
            *held += 1;
        });

    struct Memo { int key; mutable int cached; };
    threadsafe::synchronized_value<Memo> memo_value{Memo{0, 0}};
    static_assert(std::is_same_v<decltype(memo_value)::mutex, std::mutex>);
    const double memo_guarded =
        best_nanoseconds_per_iteration(rounds, lock_iterations, [&] {
            auto held = memo_value.lock();
            held->key += 1;
        });

    std::printf("uncontended write lock/unlock, best of %d x %d\n",
                rounds, lock_iterations);
    std::printf("  std::mutex + lock_guard                      %6.2f ns\n", plain);
    std::printf("  std::shared_mutex + unique_lock              %6.2f ns\n",
                shared_exclusive);
    std::printf("  synchronized_value<int>.lock()  [shared_mutex] %6.2f ns\n",
                guarded);
    std::printf("  synchronized_value<Memo>.lock() [mutex]        %6.2f ns\n",
                memo_guarded);

    constexpr int launch_rounds = 3;
    constexpr int launch_iterations = 1000;

    const double raw_launch =
        best_nanoseconds_per_iteration(launch_rounds, launch_iterations, [&] {
            std::jthread worker([] {});
        });

    const double launcher_launch =
        best_nanoseconds_per_iteration(launch_rounds, launch_iterations, [&] {
            threadsafe::asynchronous_task_launcher launcher;
            launcher.launch_task([] {});
        });

    std::printf("\nlaunch + join one empty task, best of %d x %d\n",
                launch_rounds, launch_iterations);
    std::printf("  raw std::jthread                             %8.0f ns\n",
                raw_launch);
    std::printf("  asynchronous_task_launcher::launch_task      %8.0f ns\n",
                launcher_launch);
    std::printf("\n(sink %d %d)\n", plain_value, shared_value);
}
