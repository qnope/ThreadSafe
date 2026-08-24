// Producer / consumer, first attempt: a hand-written blocking queue built from
// the standard primitives, shared through a shared_ptr. Nothing in it is
// unsound; every member does its own synchronisation.
#include <threadsafe/threadsafe.h>

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <print>

template <class T>
class blocking_queue {
public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> held{mutex_};
            items_.push_back(std::move(value));
        }
        not_empty_.notify_one();
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> held{mutex_};
        not_empty_.wait(held, [this] { return closed_ || !items_.empty(); });
        if (items_.empty())
            return std::nullopt;
        T value = std::move(items_.front());
        items_.pop_front();
        return value;
    }

    void close() {
        {
            std::lock_guard<std::mutex> held{mutex_};
            closed_ = true;
        }
        not_empty_.notify_all();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::deque<T> items_;
    bool closed_ = false;
};

// The escape hatch a template class needs: the macro only makes FULL
// specialisations, so this has to be hand-written.
template <class T>
struct threadsafe::is_synchronizable<blocking_queue<T>>
    : threadsafe::is_sendable<T> {};
template <class T>
struct threadsafe::is_lifetime_aware<blocking_queue<T>>
    : threadsafe::is_lifetime_aware<T> {};

int main() {
    auto queue = std::make_shared<blocking_queue<int>>();

    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(
        [](std::shared_ptr<blocking_queue<int>> consumed) {
            while (auto item = consumed->pop())
                std::println("got {}", *item);
        },
        queue);

    for (int produced = 0; produced < 3; ++produced)
        queue->push(produced);
    queue->close();
}
