#include <threadsafe/threadsafe.h>
#include <functional>
#include <string>
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    std::string message = "hello";
    launcher.launch_scoped_task([](std::string&) {}, std::ref(message));
}
