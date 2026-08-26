#include <threadsafe/threadsafe.h>

#include <atomic>
#include <barrier>
#include <condition_variable>
#include <functional>
#include <latch>
#include <memory>
#include <mutex>
#include <semaphore>
#include <shared_mutex>

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

// std::atomic<bool> is accepted...
static_assert(is_synchronizable_v<std::atomic<bool>>);

// ...but every other synchronization primitive the standard declares race-free
// is not. [atomics.flag], [thread.mutex.requirements.mutex], [thread.condition]
// and [thread.coord] all promise that concurrent calls to the member functions
// of these types do not introduce a data race.
static_assert(!is_synchronizable_v<std::atomic_flag>);
static_assert(!is_synchronizable_v<std::once_flag>);
static_assert(!is_synchronizable_v<std::mutex>);
static_assert(!is_synchronizable_v<std::recursive_mutex>);
static_assert(!is_synchronizable_v<std::timed_mutex>);
static_assert(!is_synchronizable_v<std::shared_mutex>);
static_assert(!is_synchronizable_v<std::condition_variable>);
static_assert(!is_synchronizable_v<std::latch>);
static_assert(!is_synchronizable_v<std::barrier<>>);
static_assert(!is_synchronizable_v<std::counting_semaphore<8>>);
static_assert(!is_synchronizable_v<std::binary_semaphore>);

// The consequence: none of them can be shared with another thread, by any
// route the library offers.
static_assert(!is_sendable_v<std::atomic_flag&>);
static_assert(!is_sendable_v<std::atomic_flag*>);
static_assert(!is_sendable_v<std::reference_wrapper<std::atomic_flag>>);
static_assert(!is_sendable_v<std::shared_ptr<std::atomic_flag>>);
static_assert(!is_sendable_v<std::reference_wrapper<std::mutex>>);
static_assert(!is_sendable_v<std::shared_ptr<std::latch>>);

// So the canonical "hand the workers a stop flag" shape is refused:
static_assert(!threadsafe::launchable_scoped_task<
                  decltype([](std::atomic_flag&) {}),
                  std::reference_wrapper<std::atomic_flag>>);

// And a user's own mutex-protected wrapper cannot even be read-shared, because
// the mutable std::mutex it holds is not synchronizable:
struct GuardedCounter {
    mutable std::mutex gate;
    int value;
};
static_assert(!is_synchronizable_v<const GuardedCounter>);

int main() {}
