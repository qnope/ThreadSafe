// "Share one synchronized_value between N threads", written the way the library
// wants it written.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <memory>
#include <vector>

using counter_type = threadsafe::synchronized_value<std::vector<int>>;

int main() {
    const std::shared_ptr<counter_type> shared_counter = counter_type::make();

    threadsafe::asynchronous_task_launcher launcher;
    for (int worker_index = 0; worker_index < 8; ++worker_index)
        launcher.launch_task(
            [](std::shared_ptr<counter_type> counter, int index) {
                for (int step = 0; step < 1000; ++step) {
                    const auto guard = counter->lock();
                    guard->push_back(index);
                }
            },
            shared_counter, worker_index);

    // The launcher joins in its destructor (std::jthread members).
    return 0;
}
