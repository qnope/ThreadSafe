#include <threadsafe/threadsafe.h>

#include <atomic>
#include <functional>
#include <latch>
#include <mutex>

void count_down_on(std::latch &gate) { gate.count_down(); }
void lock_and_touch(std::mutex &m, std::atomic<int> &n) {
    std::lock_guard guard{m};
    n.fetch_add(1);
}

int main() {
    std::latch gate{2};
    std::mutex m;
    std::atomic<int> n{0};
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_scoped_task(&count_down_on, std::ref(gate));
    launcher.launch_scoped_task(&lock_and_touch, std::ref(m), std::ref(n));
    gate.count_down();
}
