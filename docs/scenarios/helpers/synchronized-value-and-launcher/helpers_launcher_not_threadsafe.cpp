#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <thread>

static_assert(!threadsafe::is_synchronizable_v<threadsafe::asynchronous_task_launcher>);

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    std::thread first([&launcher] {
        for (int i = 0; i < 200; ++i) launcher.launch_task([] {});
    });
    std::thread second([&launcher] {
        for (int i = 0; i < 200; ++i) launcher.launch_task([] {});
    });
    first.join();
    second.join();
    std::printf("both producers done\n");
}
