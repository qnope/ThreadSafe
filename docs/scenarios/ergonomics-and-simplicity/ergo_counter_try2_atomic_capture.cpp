// (a) attempt 2: "the compiler complained, so the int must not be thread-safe" --
// make it std::atomic and keep the capture.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>

int main() {
    std::atomic<int> counter{0};

    threadsafe::asynchronous_task_launcher launcher;
    for (int worker_index = 0; worker_index < 4; ++worker_index)
        launcher.launch_task([&counter] {
            for (int step = 0; step < 1000; ++step)
                ++counter;
        });

    std::printf("%d\n", counter.load());
}
