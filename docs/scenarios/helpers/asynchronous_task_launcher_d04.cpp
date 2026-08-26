#include <threadsafe/threadsafe.h>
#include <string_view>
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](std::string_view text) { (void)text; }, "hello");
}
