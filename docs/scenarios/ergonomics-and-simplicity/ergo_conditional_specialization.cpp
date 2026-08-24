// "Synchronizable only when this template parameter is": a user-written
// mutex-protected box, plus a lock-free box that is only safe for atomics.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace app {

template <class T>
class guarded_box {
public:
    T copy() const {
        std::lock_guard<std::mutex> held(mutex_);
        return value_;
    }
    void assign(T next) {
        std::lock_guard<std::mutex> held(mutex_);
        value_ = std::move(next);
    }

private:
    mutable std::mutex mutex_;
    T value_;
};

template <class T>
class raw_box {
public:
    T value;
};

}

// A mutex serialises the accesses, so the box is synchronizable exactly when
// the value may cross a thread boundary at all.
template <class T>
struct threadsafe::is_synchronizable<app::guarded_box<T>>
    : threadsafe::is_sendable<T> {};

// The raw box synchronizes nothing: it is only synchronizable when its
// element already is.
template <class T>
struct threadsafe::is_synchronizable<app::raw_box<T>>
    : threadsafe::is_synchronizable<T> {};

static_assert(threadsafe::is_synchronizable_v<app::guarded_box<std::string>>);
static_assert(!threadsafe::is_synchronizable_v<app::guarded_box<int *>>);
static_assert(threadsafe::is_synchronizable_v<app::raw_box<std::atomic<int>>>);
static_assert(!threadsafe::is_synchronizable_v<app::raw_box<int>>);

// And the const question follows on its own.
static_assert(threadsafe::is_synchronizable_v<const app::guarded_box<std::string>>);
static_assert(
    threadsafe::is_sendable_v<app::guarded_box<std::string> &>,
    "a reference to a synchronizable box may cross");
static_assert(threadsafe::is_sendable_v<
              std::vector<std::shared_ptr<app::guarded_box<std::string>>>>);
