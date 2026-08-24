// Capturing BY VALUE -- the idiomatic "move my data into the thread" -- is it accepted?
#include <threadsafe/threadsafe.h>

#include <string>
#include <vector>

int main() {
    std::vector<int> samples{1, 2, 3};

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([owned = std::move(samples)] {
        volatile int total = 0;
        for (int sample : owned)
            total += sample;
    });
}
