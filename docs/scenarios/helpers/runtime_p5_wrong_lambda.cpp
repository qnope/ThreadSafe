// Program 5, THE WRONG VERSION -- slide 1 of the talk.
// Four threads incrementing a shared int through a captured reference.
#include <threadsafe/threadsafe.h>
#include <cstdio>

int main() {
    int total = 0;
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < 4; ++worker)
            launcher.launch_task([&total] {
                for (int step = 0; step < 100'000; ++step)
                    ++total;
            });
    }
    std::printf("total = %d (expected 400000)\n", total);
}
