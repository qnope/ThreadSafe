#include <threadsafe/threadsafe.h>
#include <vector>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    // The task returns a borrow into a vector that dies with the task's frame.
    auto dangling = launcher.launch_value_task(
        [](std::vector<int> numbers) { return numbers.data(); },
        std::vector<int>(10));
    (void)dangling;
}
