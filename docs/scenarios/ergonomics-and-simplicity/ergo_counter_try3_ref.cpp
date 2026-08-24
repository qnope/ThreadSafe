// (a) attempt 3: pass the atomic as an argument with std::ref, the way
// std::thread has always been used.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <functional>

int main() {
    std::atomic<int> counter{0};

    threadsafe::asynchronous_task_launcher launcher;
    for (int worker_index = 0; worker_index < 4; ++worker_index)
        launcher.launch_task(
            [](std::atomic<int>& shared_counter) {
                for (int step = 0; step < 1000; ++step)
                    ++shared_counter;
            },
            std::ref(counter));

    std::printf("%d\n", counter.load());
}
