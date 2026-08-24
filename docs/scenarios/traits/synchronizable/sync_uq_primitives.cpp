#include <threadsafe/threadsafe.h>
#include <atomic>
#include <barrier>
#include <condition_variable>
#include <latch>
#include <mutex>
#include <semaphore>
#include <shared_mutex>
using threadsafe::is_synchronizable_v;
static_assert(is_synchronizable_v<std::atomic_flag>);
static_assert(is_synchronizable_v<std::mutex>);
static_assert(is_synchronizable_v<std::shared_mutex>);
static_assert(is_synchronizable_v<std::condition_variable>);
static_assert(is_synchronizable_v<std::latch>);
static_assert(is_synchronizable_v<std::counting_semaphore<>>);
static_assert(is_synchronizable_v<std::barrier<>>);
static_assert(threadsafe::is_sendable_v<std::mutex&>);
struct Guarded { mutable std::mutex m; int v; };
static_assert(is_synchronizable_v<const Guarded>);
