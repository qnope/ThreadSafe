// Every trait holds; the callable simply does not accept the argument.
// launch_task's constrained overload therefore wins, and the error is whatever
// std::jthread produces.
#include <threadsafe/threadsafe.h>

#include <string>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](int value) { (void)value; }, std::string{"oops"});
}
