#include <threadsafe/threadsafe.h>

#include <functional>

int main() {
    std::function<void()> task = [] {};
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_scoped_task(task);
}
