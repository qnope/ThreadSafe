// Proposed helper: a borrowing scope whose tasks run at the same time.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <print>
#include <thread>

int main() {
    std::atomic<int> shared_total{0};

    const auto started = std::chrono::steady_clock::now();
    {
        threadsafe::scoped_task_scope scope;
        for (int task_index = 0; task_index < 4; ++task_index)
            scope.launch(
                [](std::atomic<int> &total) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    total.fetch_add(1, std::memory_order_relaxed);
                },
                std::ref(shared_total));
    }
    const auto elapsed = std::chrono::steady_clock::now() - started;

    std::println("4 concurrent scoped tasks of 100 ms took {} ms, total = {}",
                 std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                     .count(),
                 shared_total.load());
}
