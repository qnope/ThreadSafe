// Q3b: does the guard's stored T* survive into the generated code?
// Same TU so GCC's heuristics are identical for both sides.
#include <threadsafe/threadsafe.h>

#include <mutex>
#include <shared_mutex>
#include <vector>

struct exclusive_only {
    mutable int cache_ = 0;
    int value_ = 0;
};

struct plain_shared {
    mutable std::shared_mutex mutex_;
    std::vector<int> data_;
};

struct plain_exclusive {
    mutable std::mutex mutex_;
    exclusive_only value_;
};

int read_shared_traits(const threadsafe::synchronized_value<std::vector<int>>& value) {
    auto guard = value.lock_shared();
    return (*guard)[0];
}

int read_shared_plain(const plain_shared& value) {
    std::shared_lock lock{value.mutex_};
    return value.data_[0];
}

void write_exclusive_traits(threadsafe::synchronized_value<exclusive_only>& value) {
    auto guard = value.lock();
    guard->value_ += 1;
}

void write_exclusive_plain(plain_exclusive& value) {
    std::unique_lock lock{value.mutex_};
    value.value_.value_ += 1;
}
