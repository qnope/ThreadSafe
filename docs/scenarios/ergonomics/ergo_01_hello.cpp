// The smallest complete program a new user can write: send a vector to a thread.
#include <threadsafe/threadsafe.h>

#include <print>
#include <string>
#include <vector>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(
        [](std::vector<int> numbers, std::string label) {
            std::println("{}: {}", label, numbers.size());
        },
        std::vector<int>{1, 2, 3}, std::string{"batch"});
}
