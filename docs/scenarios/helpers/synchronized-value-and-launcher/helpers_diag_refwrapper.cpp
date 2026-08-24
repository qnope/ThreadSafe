#include <threadsafe/threadsafe.h>
#include <atomic>
#include <functional>
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    std::atomic<int> counter{0};
    launcher.launch_task([](std::atomic<int>&) {}, std::ref(counter));
}
