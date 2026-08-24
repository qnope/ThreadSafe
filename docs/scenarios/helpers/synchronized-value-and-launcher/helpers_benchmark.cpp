#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

using clock_type = std::chrono::steady_clock;

template <class Body>
double nanoseconds_per_iteration(int iterations, Body body) {
    const auto start = clock_type::now();
    for (int step = 0; step < iterations; ++step) body();
    const auto elapsed = clock_type::now() - start;
    return std::chrono::duration<double, std::nano>(elapsed).count() / iterations;
}

int main() {
    constexpr int lock_iterations = 5'000'000;

    std::mutex plain_mutex;
    int plain_value = 0;
    const double plain = nanoseconds_per_iteration(lock_iterations, [&] {
        std::lock_guard<std::mutex> held(plain_mutex);
        plain_value += 1;
    });

    std::shared_mutex plain_shared_mutex;
    int shared_value = 0;
    const double shared = nanoseconds_per_iteration(lock_iterations, [&] {
        std::unique_lock<std::shared_mutex> held(plain_shared_mutex);
        shared_value += 1;
    });

    threadsafe::synchronized_value<int> guarded_value{0};
    const double guarded = nanoseconds_per_iteration(lock_iterations, [&] {
        auto held = guarded_value.lock();
        *held += 1;
    });

    const double guarded_read = nanoseconds_per_iteration(lock_iterations, [&] {
        auto held = guarded_value.lock_shared();
        shared_value += *held;
    });

    std::printf("uncontended write lock/unlock (ns/op, %d iterations)\n",
                lock_iterations);
    std::printf("  std::mutex + lock_guard                 %7.2f\n", plain);
    std::printf("  std::shared_mutex + unique_lock         %7.2f\n", shared);
    std::printf("  synchronized_value<int>::lock()         %7.2f\n", guarded);
    std::printf("  synchronized_value<int>::lock_shared()  %7.2f\n", guarded_read);

    constexpr int launch_iterations = 2000;

    const double raw_launch = nanoseconds_per_iteration(launch_iterations, [&] {
        std::vector<std::jthread> threads;
        threads.emplace_back([] {});
        threads.clear();
    });

    const double launcher_launch =
        nanoseconds_per_iteration(launch_iterations, [&] {
            threadsafe::asynchronous_task_launcher launcher;
            launcher.launch_task([] {});
        });

    std::printf("\nlaunch + join one empty task (ns/op, %d iterations)\n",
                launch_iterations);
    std::printf("  raw std::jthread in a vector            %9.0f\n", raw_launch);
    std::printf("  asynchronous_task_launcher::launch_task %9.0f\n",
                launcher_launch);
    std::printf("\n(sink %d %d)\n", plain_value, shared_value);
}
