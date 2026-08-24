#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>
#include <algorithm>

#include <mach/mach.h>

namespace {

std::atomic<std::uint64_t> sink{0};

struct tiny_task {
    void operator()() const { sink.fetch_add(1, std::memory_order_relaxed); }
};

std::size_t resident_bytes() {
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  (task_info_t)&info, &count) != KERN_SUCCESS)
        return 0;
    return info.resident_size;
}

std::size_t live_thread_count() {
    thread_act_array_t threads{};
    mach_msg_type_number_t count = 0;
    if (task_threads(mach_task_self(), &threads, &count) != KERN_SUCCESS)
        return 0;
    vm_deallocate(mach_task_self(), (vm_address_t)threads,
                  count * sizeof(thread_act_t));
    return count;
}

double seconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

}

int main() {
    std::printf("sizeof(std::jthread) = %zu\n\n", sizeof(std::jthread));

    const std::size_t baseline = resident_bytes();
    std::printf("baseline RSS = %.2f MiB, live OS threads = %zu\n\n",
                baseline / 1048576.0, live_thread_count());

    for (int task_count : {1000, 5000, 20000, 50000}) {
        const auto start = std::chrono::steady_clock::now();
        std::size_t peak_rss = 0;
        std::size_t peak_threads = 0;
        {
            threadsafe::asynchronous_task_launcher launcher;
            for (int index = 0; index < task_count; ++index) {
                launcher.launch_task(tiny_task{});
                if ((index & 511) == 0) {
                    peak_rss = std::max(peak_rss, resident_bytes());
                    peak_threads = std::max(peak_threads, live_thread_count());
                }
            }
            peak_rss = std::max(peak_rss, resident_bytes());
            peak_threads = std::max(peak_threads, live_thread_count());
            std::printf("launcher, %6d tiny tasks: spawn %7.3f s, "
                        "peak RSS %8.2f MiB (+%7.2f MiB), peak live OS threads %6zu\n",
                        task_count, seconds_since(start), peak_rss / 1048576.0,
                        (peak_rss - baseline) / 1048576.0, peak_threads);
        }
        std::printf("            after ~destructor: %7.3f s total, RSS %8.2f MiB, live OS threads %6zu\n",
                    seconds_since(start), resident_bytes() / 1048576.0, live_thread_count());

        const auto start_joined = std::chrono::steady_clock::now();
        std::size_t peak_threads_joined = 0;
        for (int index = 0; index < task_count; ++index) {
            std::jthread thread{tiny_task{}};
            if ((index & 511) == 0)
                peak_threads_joined = std::max(peak_threads_joined, live_thread_count());
        }
        std::printf("            join-as-you-go equivalent: %7.3f s, RSS %8.2f MiB, peak live OS threads %6zu\n\n",
                    seconds_since(start_joined), resident_bytes() / 1048576.0,
                    peak_threads_joined);
    }

    std::printf("sink=%llu\n", (unsigned long long)sink.load());
}
