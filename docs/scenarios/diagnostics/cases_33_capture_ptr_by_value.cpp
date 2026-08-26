#include <threadsafe/threadsafe.h>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    int counter = 0;
    int *pointer = &counter;
    launcher.launch_task([pointer] { *pointer = 1; });
}
