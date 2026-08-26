// Program 8 -- launch_scoped_task is not asynchronous.
// It is the only entry point that accepts a reference (launchable_scoped_task
// drops the lifetime_aware requirement), so it is the only way to fan out over
// borrowed data. Its body is
//     std::jthread task{std::move(f), std::move(args)...};
//     task.join();
// -- a thread created and immediately joined, so N calls run one after another.
//
// build: g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread \
//            p8_scoped_task.cpp -o p8 && ./p8
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <thread>
#include <vector>

namespace {

constexpr int task_count = 8;
constexpr auto work_duration = std::chrono::milliseconds(100);

struct sleeping_task {
    void operator()(std::atomic<int>& peak_concurrency,
                    std::atomic<int>& live) const {
        const int now = ++live;
        int previous = peak_concurrency.load();
        while (previous < now
               && !peak_concurrency.compare_exchange_weak(previous, now))
            ;
        std::this_thread::sleep_for(work_duration);
        --live;
    }
};

double milliseconds_of(auto&& action) {
    const auto started_at = std::chrono::steady_clock::now();
    action();
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - started_at)
        .count();
}

}

// launch_task refuses a reference argument; launch_scoped_task accepts it.
static_assert(!threadsafe::launchable_task<
                  sleeping_task, std::reference_wrapper<std::atomic<int>>,
                  std::reference_wrapper<std::atomic<int>>>);
static_assert(threadsafe::launchable_scoped_task<
                  sleeping_task, std::reference_wrapper<std::atomic<int>>,
                  std::reference_wrapper<std::atomic<int>>>);

int main() {
    std::atomic<int> peak_concurrency{0};
    std::atomic<int> live{0};

    const double scoped_milliseconds = milliseconds_of([&] {
        threadsafe::asynchronous_task_launcher launcher;
        for (int task = 0; task < task_count; ++task)
            launcher.launch_scoped_task(sleeping_task{},
                                        std::ref(peak_concurrency),
                                        std::ref(live));
    });
    const int scoped_peak = peak_concurrency.exchange(0);

    const double bare_milliseconds = milliseconds_of([&] {
        std::vector<std::jthread> threads;
        for (int task = 0; task < task_count; ++task)
            threads.emplace_back([&] {
                sleeping_task{}(peak_concurrency, live);
            });
    });
    const int bare_peak = peak_concurrency.load();

    std::printf("%d tasks, each sleeping %lld ms\n\n", task_count,
                (long long)work_duration.count());
    std::printf("launch_scoped_task x%d : %7.1f ms   peak threads running at "
                "once: %d\n",
                task_count, scoped_milliseconds, scoped_peak);
    std::printf("bare std::jthread  x%d : %7.1f ms   peak threads running at "
                "once: %d\n",
                task_count, bare_milliseconds, bare_peak);
}
