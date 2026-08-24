// Same work, hand-written: shared_ptr<mutex+int> + stateful lambda into a jthread vector.
#include <memory>
#include <shared_mutex>
#include <thread>
#include <vector>

void consume(int total);

struct plain_counter {
    mutable std::shared_mutex mutex_;
    int value_;
    explicit plain_counter(int value) : value_(value) {}
};

struct plain_launcher {
    std::vector<std::jthread> threads_;
};

void run(plain_launcher& launcher,
         std::shared_ptr<plain_counter> counter,
         int increment) {
    launcher.threads_.emplace_back(
        [](std::shared_ptr<plain_counter> shared, int step) {
            std::unique_lock lock{shared->mutex_};
            shared->value_ += step;
            consume(shared->value_);
        },
        std::move(counter), increment);
}
