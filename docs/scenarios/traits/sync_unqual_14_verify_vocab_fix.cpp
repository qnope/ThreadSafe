#include <threadsafe/threadsafe.h>
#include <atomic>
#include <barrier>
#include <functional>
#include <latch>
#include <memory>
#include <mutex>
#include <semaphore>
#include <shared_mutex>

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(is_synchronizable_v<std::atomic_flag>);
static_assert(is_synchronizable_v<std::once_flag>);
static_assert(is_synchronizable_v<std::mutex>);
static_assert(is_synchronizable_v<std::shared_mutex>);
static_assert(is_synchronizable_v<std::condition_variable>);
static_assert(is_synchronizable_v<std::latch>);
static_assert(is_synchronizable_v<std::barrier<>>);
static_assert(is_synchronizable_v<std::counting_semaphore<8>>);
static_assert(is_synchronizable_v<std::binary_semaphore>);
static_assert(is_synchronizable_v<std::atomic_ref<int>>);
static_assert(!is_synchronizable_v<std::atomic_ref<int*>>);

static_assert(is_sendable_v<std::atomic_flag&>);
static_assert(is_sendable_v<std::atomic_ref<int>>);
static_assert(is_sendable_v<std::reference_wrapper<std::mutex>>);
static_assert(is_sendable_v<std::shared_ptr<std::atomic_flag>>);
static_assert(is_synchronizable_v<const std::atomic_flag>);

// The classic user-written mutex wrapper now passes the const question on its own.
struct GuardedCounter {
    mutable std::mutex gate;
    int value;
};
static_assert(is_synchronizable_v<const GuardedCounter>);

// and a mutable non-synchronizing member is still refused
struct LeakyCounter {
    mutable std::mutex gate;
    mutable int cached;
};
static_assert(!is_synchronizable_v<const LeakyCounter>);

static_assert(threadsafe::launchable_scoped_task<decltype([](std::atomic_flag&) {}),
                                          std::reference_wrapper<std::atomic_flag>>);
int main() {}
