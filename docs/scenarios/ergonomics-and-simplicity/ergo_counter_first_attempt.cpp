// The first thing anyone writes: an atomic counter on the stack, incremented by
// four tasks that capture it by reference.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <print>

int main() {
    std::atomic<int> counter{0};

    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker_index = 0; worker_index < 4; ++worker_index)
            launcher.launch_task([&counter] {
                for (int step = 0; step < 1000; ++step)
                    counter.fetch_add(1, std::memory_order_relaxed);
            });
    }

    std::println("counter = {}", counter.load());
}
