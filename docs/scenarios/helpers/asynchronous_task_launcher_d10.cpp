#include <threadsafe/threadsafe.h>
#include <string>
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    // every trait holds; the callable simply is not invocable with the argument
    launcher.launch_task([](int) {}, std::string{"oops"});
}
