#include <threadsafe/threadsafe.h>
#include <functional>
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    std::function<void()> task = [] {};
    launcher.launch_task(task);
}
