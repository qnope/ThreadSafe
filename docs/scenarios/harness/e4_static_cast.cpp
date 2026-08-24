#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <memory>
using sync_int = threadsafe::synchronized_value<int>;
int main() {
    auto shared_counter = sync_int::make(0);
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int worker = 0; worker < 2; ++worker)
            launcher.launch_task([](std::shared_ptr<sync_int> counter) {
                for (int step = 0; step < 100000; ++step) {
                    int& escaped = *static_cast<const sync_int::guard&>(counter->lock());
                    escaped += 1;
                }
            }, shared_counter);
    }
    auto final_guard = shared_counter->lock();
    std::printf("counter = %d (expected 200000)\n", *final_guard);
}
