#include "perf_bench.h"

#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

#include <mach/mach.h>

namespace {

std::atomic<long long> sink{0};
void short_task(int value) { sink.fetch_add(value, std::memory_order_relaxed); }

double resident_megabytes() {
    task_vm_info_data_t info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    task_info(mach_task_self(), TASK_VM_INFO,
              reinterpret_cast<task_info_t>(&info), &count);
    return double(info.phys_footprint) / (1024.0 * 1024.0);
}

}

int main() {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::printf("%9s | %14s | %14s | %14s | %14s | %14s\n", "tasks",
                "launcher ms", "launcher MB", "reserve() ms", "reaped ms",
                "reaped MB");
    std::printf("--------------------------------------------------------------"
                "-------------------------------------\n");
    for (int task_count : {1000, 4000, 8000, 16000, 32000}) {
        double launcher_ms = 0.0, launcher_mb = 0.0;
        {
            const double before = resident_megabytes();
            threadsafe::asynchronous_task_launcher launcher;
            const auto start = bench::clock_type::now();
            for (int index = 0; index < task_count; ++index)
                launcher.launch_task(short_task, 1);
            const auto stop = bench::clock_type::now();
            launcher_ms =
                std::chrono::duration<double, std::milli>(stop - start).count();
            launcher_mb = resident_megabytes() - before;
        }

        double reserve_ms = 0.0;
        {
            std::vector<std::jthread> threads;
            threads.reserve(std::size_t(task_count));
            const auto start = bench::clock_type::now();
            for (int index = 0; index < task_count; ++index)
                threads.emplace_back(short_task, 1);
            const auto stop = bench::clock_type::now();
            reserve_ms =
                std::chrono::duration<double, std::milli>(stop - start).count();
        }

        double reaped_ms = 0.0, reaped_mb = 0.0;
        {
            const double before = resident_megabytes();
            std::vector<std::jthread> threads;
            const auto start = bench::clock_type::now();
            for (int index = 0; index < task_count; ++index) {
                threads.emplace_back(short_task, 1);
                if (threads.size() >= 12)
                    threads.clear();
            }
            threads.clear();
            const auto stop = bench::clock_type::now();
            reaped_ms =
                std::chrono::duration<double, std::milli>(stop - start).count();
            reaped_mb = resident_megabytes() - before;
        }

        std::printf("%9d | %14.1f | %14.1f | %14.1f | %14.1f | %14.1f\n",
                    task_count, launcher_ms, launcher_mb, reserve_ms, reaped_ms,
                    reaped_mb);
    }
    bench::do_not_optimize(sink.load());
}
