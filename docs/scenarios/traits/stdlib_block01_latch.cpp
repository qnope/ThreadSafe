#include <threadsafe/threadsafe.h>

#include <chrono>
#include <functional>
#include <latch>

void count_down_on(std::latch &gate) { gate.count_down(); }

int main() {
    std::latch gate{2};
    threadsafe::asynchronous_task_launcher launcher;
    // std::latch is designed for exactly this: [thread.latch] guarantees that
    // concurrent invocations of its member functions do not race.
    launcher.launch_task(&count_down_on, std::ref(gate));
}
