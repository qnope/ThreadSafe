#include <threadsafe/threadsafe.h>
#include <cstdio>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    int counter = 7;
    launcher.launch_task([counter] { std::printf("%d\n", counter); });
}
