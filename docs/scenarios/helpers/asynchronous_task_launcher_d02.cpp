#include <threadsafe/threadsafe.h>
#include <string>
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    std::string message = "hello";
    launcher.launch_task([message] { (void)message.size(); });
}
