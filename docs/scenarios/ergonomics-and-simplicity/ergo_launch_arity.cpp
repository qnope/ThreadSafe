// Every trait holds; the call is simply wrong — the callable takes two
// arguments and one is supplied.
#include <threadsafe/threadsafe.h>

#include <string>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](int first, std::string second) {}, 42);
}
