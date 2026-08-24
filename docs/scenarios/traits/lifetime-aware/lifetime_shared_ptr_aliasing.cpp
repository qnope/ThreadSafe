#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

using SharedCounter = std::shared_ptr<std::atomic<int>>;

static_assert(threadsafe::launchable_task<void (*)(SharedCounter), SharedCounter>);

void bump_later(SharedCounter counter) {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    counter->fetch_add(1, std::memory_order_relaxed);
    std::printf("worker bumped a counter that no longer exists\n");
}

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    auto unrelated_owner = std::make_shared<int>(7);
    {
        std::atomic<int> stack_counter{0};
        SharedCounter aliasing_pointer(unrelated_owner, &stack_counter);
        launcher.launch_task(&bump_later, aliasing_pointer);
        std::printf("main leaves the scope; stack_counter dies\n");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    return 0;
}
