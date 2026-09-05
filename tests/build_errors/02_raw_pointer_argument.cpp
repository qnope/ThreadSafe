#include <threadsafe/threadsafe.h>

#include <atomic>

struct Counter {
    std::atomic<int> ticks{0};
};

template <>
struct threadsafe::is_unsafe_synchronizable<Counter> : std::true_type {};

int main() {
    Counter counter;
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](Counter *) {}, &counter);
}
