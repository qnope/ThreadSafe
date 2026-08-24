// Beginner attempt #2: "capture is refused, so I'll pass it by std::ref".
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <functional>
#include <print>

int main() {
    std::atomic<int> counter{0};
    threadsafe::asynchronous_task_launcher launcher;

    launcher.launch_task(
        [](std::atomic<int> &shared_counter) {
            for (int step = 0; step < 1000; ++step)
                shared_counter.fetch_add(1, std::memory_order_relaxed);
        },
        std::ref(counter));

    std::println("{}", counter.load());
}
