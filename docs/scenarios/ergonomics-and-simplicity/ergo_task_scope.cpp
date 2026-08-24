// The same two 500 ms tasks, launched into a scope that joins at the closing brace.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <print>
#include <thread>

int main() {
    std::atomic<int> shared_counter{0};
    const auto started_at = std::chrono::steady_clock::now();

    {
        threadsafe::task_scope scope;
        for (int worker_index = 0; worker_index < 2; ++worker_index)
            scope.launch(
                [](std::atomic<int> &counter) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{500});
                    counter.fetch_add(1, std::memory_order_relaxed);
                },
                std::ref(shared_counter));
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at);
    std::println("two 500 ms scoped tasks took {} ms, counter = {}",
                 elapsed.count(), shared_counter.load());
}
