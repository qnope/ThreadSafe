#include <threadsafe/threadsafe.h>

#include <chrono>
#include <thread>

void poll_for(std::chrono::milliseconds timeout) {
    std::this_thread::sleep_for(timeout);
}

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(&poll_for, std::chrono::milliseconds{5});
}
