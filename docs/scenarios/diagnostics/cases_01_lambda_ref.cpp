#include <threadsafe/threadsafe.h>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    int counter = 0;
    launcher.launch_task([&counter] { counter++; });
}
