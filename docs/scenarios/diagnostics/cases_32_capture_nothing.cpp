#include <threadsafe/threadsafe.h>
#include <cstdio>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([] { std::printf("hi\n"); });
}
