// (a) Share a counter across threads -- the naive version, written without
// reading any documentation.
#include <threadsafe/threadsafe.h>

#include <cstdio>

int main() {
    int counter = 0;

    threadsafe::asynchronous_task_launcher launcher;
    for (int worker_index = 0; worker_index < 4; ++worker_index)
        launcher.launch_task([&counter] {
            for (int step = 0; step < 1000; ++step)
                ++counter;
        });

    std::printf("%d\n", counter);
}
