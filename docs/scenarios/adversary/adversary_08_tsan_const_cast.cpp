// Hand-extracted plain-C++ equivalent (no reflection): synchronized_value's
// runtime body instantiated with the shared_mutex/shared_lock branch that
// ThreadSafe selected because is_synchronizable_v<const LazyTable> answered true.

#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <vector>

template <class T, class Lock>
class value_guard {
public:
    value_guard(typename Lock::mutex_type& mutex, T& value)
        : lock_(mutex), value_(&value) {}
    value_guard(const value_guard&) = delete;
    T* operator->() const& noexcept { return value_; }

private:
    Lock lock_;
    T* value_;
};

template <class T>
class synchronized_value {
public:
    using mutex = std::shared_mutex;
    using const_guard = value_guard<const T, std::shared_lock<mutex>>;

    template <class... Args>
    explicit synchronized_value(Args&&... args) : value_(std::forward<Args>(args)...) {}

    [[nodiscard]] const_guard lock_shared() const { return const_guard{mutex_, value_}; }

private:
    mutable mutex mutex_;
    T value_;
};

struct LazyTable {
    std::vector<int> rows;
    bool ready = false;

    const std::vector<int>& get() const {
        if (!ready) {
            auto& self = *const_cast<LazyTable*>(this);
            self.rows.assign(4096, 7);
            self.ready = true;
        }
        return rows;
    }
};

int main() {
    synchronized_value<LazyTable> table;

    std::vector<std::thread> workers;
    for (int i = 0; i < 2; ++i)
        workers.emplace_back([&table] {

            auto reader = table.lock_shared();
            long sum = 0;
            for (int value : reader->get()) sum += value;
            std::printf("sum = %ld\n", sum);
        });
    for (auto& worker : workers) worker.join();
}
