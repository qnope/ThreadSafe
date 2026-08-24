// The "hello world" of the library, written the way a beginner writes it:
// a counter on the stack, captured by reference, incremented by two tasks.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <print>

int main() {
    std::atomic<int> counter{0};
    threadsafe::asynchronous_task_launcher launcher;

    launcher.launch_task([&counter] {
        for (int index = 0; index < 1000; ++index)
            counter.fetch_add(1, std::memory_order_relaxed);
    });

    std::println("{}", counter.load());
}
