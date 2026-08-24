// The counter again, using the library's own synchronized_value rather than a
// raw std::atomic -- the idiom a reader of the headers would reach for.
#include <threadsafe/threadsafe.h>

#include <memory>
#include <print>

using counter_type = threadsafe::synchronized_value<int>;

int main() {
    auto shared_counter = counter_type::make(0);

    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int thread_index = 0; thread_index < 4; ++thread_index)
            launcher.launch_task(
                [](std::shared_ptr<counter_type> counter) {
                    for (int step = 0; step < 100000; ++step) {
                        auto guard = counter->lock();
                        ++*guard;
                    }
                },
                shared_counter);
    }

    const auto guard = shared_counter->lock();
    std::println("counter = {}", *guard);
}
