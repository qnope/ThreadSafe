// Hand-extracted plain-C++ equivalent of the ThreadSafe scenario (no reflection):
// synchronized_value's runtime body, instantiated with the shared_mutex branch
// that ThreadSafe selected because is_synchronizable_v<const shared_ptr<Base>>
// answered true -- an answer it reads off Base, never off the dynamic type.
#include <atomic>
#include <cstdio>
#include <memory>
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

struct Base {
    virtual ~Base() = default;
    virtual void bump() { count_.fetch_add(1, std::memory_order_relaxed); }
    std::atomic<int> count_{0};
};

struct Derived : Base {
    void bump() override { Base::bump(); ++unsynchronized_; }
    long unsynchronized_ = 0;
};

int main() {
    auto derived = std::make_shared<Derived>();
    synchronized_value<std::shared_ptr<Base>> shared{derived};

    std::vector<std::thread> workers;
    for (int i = 0; i < 2; ++i)
        workers.emplace_back([&shared] {
            for (long n = 0; n < 20000; ++n) {
                auto reader = shared.lock_shared();
                reader->get()->bump();
            }
        });
    for (auto& worker : workers) worker.join();
    std::printf("derived member: %ld (expected 40000)\n", derived->unsynchronized_);
}
