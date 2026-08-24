// Second attempt: capture a shared_ptr<atomic<int>> BY VALUE. The closure owns a
// handle to a synchronizable object — nothing is borrowed, nothing dangles.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <print>

int main() {
    auto shared_counter = std::make_shared<std::atomic<int>>(0);

    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker_index = 0; worker_index < 4; ++worker_index)
            launcher.launch_task([shared_counter] {
                for (int step = 0; step < 1000; ++step)
                    shared_counter->fetch_add(1, std::memory_order_relaxed);
            });
    }

    std::println("counter = {}", shared_counter->load());
}
