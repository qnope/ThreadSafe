// The standard "wait for N workers" shape. std::latch is designed to be used
// from many threads at once; that is its entire purpose.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <latch>
#include <memory>
#include <print>

int main() {
    auto all_done = std::make_shared<std::latch>(4);

    threadsafe::asynchronous_task_launcher launcher;
    for (int worker_index = 0; worker_index < 4; ++worker_index)
        launcher.launch_task(
            [](std::shared_ptr<std::latch> done) { done->count_down(); },
            all_done);

    all_done->wait();
    std::println("all workers reported");
}
