#include <threadsafe/threadsafe.h>
#include <span>
#include <vector>

int main() {
    std::vector<int> samples(1000);
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](std::span<const int> slice) { (void)slice; },
                         std::span<const int>{samples}.first(100));
}
