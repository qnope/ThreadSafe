// The workaround the closure diagnostic should be recommending: a named
// struct, whose members reflection CAN see and therefore check.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <print>

struct increment_task {
    std::shared_ptr<std::atomic<int>> counter;
    int repeat_count;

    void operator()() const {
        for (int step = 0; step < repeat_count; ++step)
            counter->fetch_add(1, std::memory_order_relaxed);
    }
};

int main() {
    auto shared_counter = std::make_shared<std::atomic<int>>(0);
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int thread_index = 0; thread_index < 4; ++thread_index)
            launcher.launch_task(increment_task{shared_counter, 100000});
    }
    std::println("counter = {}", shared_counter->load());
}
