#include <threadsafe/threadsafe.h>

#include <string>

int main() {
    std::string message = "hello";
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([&message] { (void) message; });
}
