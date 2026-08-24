// Trait-heavy launch: shared_ptr<synchronized_value<int>> argument + stateful lambda.
#include <threadsafe/threadsafe.h>
#include <memory>

void consume(int total);

void run(threadsafe::asynchronous_task_launcher& launcher,
         std::shared_ptr<threadsafe::synchronized_value<int>> counter,
         int increment) {
    launcher.launch_task(
        [](std::shared_ptr<threadsafe::synchronized_value<int>> shared, int step) {
            auto guard = shared->lock();
            *guard += step;
            consume(*guard);
        },
        std::move(counter), increment);
}
