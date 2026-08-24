// launch_scoped_task is the only entry point that accepts a borrowed reference.
// Does it give any parallelism?
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <thread>

namespace {
struct WorkUnit {
    std::atomic<int> completed{0};
    void run() const {}
};
}

template <>
struct threadsafe::is_synchronizable<WorkUnit> : std::true_type {};

int main() {
    WorkUnit work_unit;

    const auto started_at = std::chrono::steady_clock::now();
    threadsafe::asynchronous_task_launcher launcher;
    for (int worker_index = 0; worker_index < 4; ++worker_index)
        launcher.launch_scoped_task(
            [](WorkUnit& shared_work_unit) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                ++shared_work_unit.completed;
            },
            std::ref(work_unit));
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_at)
            .count();

    std::printf("four 200ms scoped tasks took %lldms (parallel would be ~200)\n",
                (long long) elapsed_ms);
}
