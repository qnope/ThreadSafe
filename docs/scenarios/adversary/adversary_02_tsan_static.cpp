// Hand-extracted plain-C++ equivalent of the ThreadSafe scenario, with no
// reflection: synchronized_value's runtime body, instantiated with the
// shared_mutex/shared_lock branch that ThreadSafe selected for LookupTable
// because is_synchronizable_v<const LookupTable> answered true.
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
    using mutex = std::shared_mutex;                       // ThreadSafe's choice
    using const_guard = value_guard<const T, std::shared_lock<mutex>>;

    template <class... Args>
    explicit synchronized_value(Args&&... args) : value_(std::forward<Args>(args)...) {}

    [[nodiscard]] const_guard lock_shared() const { return const_guard{mutex_, value_}; }

private:
    mutable mutex mutex_;
    T value_;
};

class LookupTable {
public:
    int find(int key) const {
        ++probe_count_;
        return key * 2;
    }
    static long probes() { return probe_count_; }

private:
    static inline long probe_count_ = 0;
};

int main() {
    synchronized_value<LookupTable> table;
    std::vector<std::thread> workers;
    for (int i = 0; i < 2; ++i)
        workers.emplace_back([&table] {
            for (long n = 0; n < 20000; ++n) {
                auto reader = table.lock_shared();
                (void)reader->find(static_cast<int>(n));
            }
        });
    for (auto& worker : workers) worker.join();
    std::printf("observed %ld probes (expected 40000)\n", LookupTable::probes());
}
