// Every trait holds; the callable simply does not accept the argument.
#include <threadsafe/threadsafe.h>

#include <string>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](int job_index) { (void)job_index; },
                         std::string{"not an int"});
}
