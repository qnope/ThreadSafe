// The "hello world" of a thread-safety library, written the way a newcomer
// writes it: a counter, four threads, capture it by reference.
#include <threadsafe/threadsafe.h>

#include <print>

int main() {
    int counter = 0;

    threadsafe::asynchronous_task_launcher launcher;
    for (int thread_index = 0; thread_index < 4; ++thread_index)
        launcher.launch_task([&counter] {
            for (int step = 0; step < 100000; ++step)
                ++counter;
        });
}
