// Q3c: the one case where the guard's T* could survive -- a guard held live
// across an opaque call, so SROA cannot scalarise it away.
#include <threadsafe/threadsafe.h>
#include <mutex>

struct exclusive_only {
    mutable int cache_ = 0;
    int value_ = 0;
};

struct plain_exclusive {
    mutable std::mutex mutex_;
    exclusive_only value_;
};

void opaque();

void hold_traits(threadsafe::synchronized_value<exclusive_only>& value) {
    auto guard = value.lock();
    opaque();
    guard->value_ += 1;
    opaque();
    guard->value_ += 2;
}

void hold_plain(plain_exclusive& value) {
    std::unique_lock lock{value.mutex_};
    opaque();
    value.value_.value_ += 1;
    opaque();
    value.value_.value_ += 2;
}
