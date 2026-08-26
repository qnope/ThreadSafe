#include <threadsafe/threadsafe.h>
#include <memory>
#include <vector>

int main() {
    auto table = std::make_shared<const std::vector<int>>(std::vector<int>{1, 2, 3});
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](std::shared_ptr<const std::vector<int>> shared) {
        (void)shared->size();
    }, table);
}
