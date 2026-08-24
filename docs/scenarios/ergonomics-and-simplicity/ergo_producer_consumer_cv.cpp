// Producer / consumer, the textbook way: a queue behind a mutex plus a
// condition_variable so the consumer sleeps instead of spinning.
#include <threadsafe/threadsafe.h>

#include <condition_variable>
#include <deque>
#include <memory>
#include <print>

int main() {
    threadsafe::synchronized_value<std::deque<int>> queue;
    std::condition_variable_any items_available;

    auto guard = queue.lock();
    items_available.wait(guard, [&] { return !guard->empty(); });
    std::println("{}", guard->front());
}
