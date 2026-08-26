#include <threadsafe/threadsafe.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <latch>
#include <mutex>
#include <semaphore>
#include <shared_mutex>
#include <thread>

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

// The canonical Sync types of the standard library are not synchronizable.
static_assert(!is_synchronizable_v<std::mutex>);
static_assert(!is_synchronizable_v<std::shared_mutex>);
static_assert(!is_synchronizable_v<std::recursive_mutex>);
static_assert(!is_synchronizable_v<std::condition_variable>);
static_assert(!is_synchronizable_v<std::latch>);
static_assert(!is_synchronizable_v<std::binary_semaphore>);
static_assert(!is_synchronizable_v<std::atomic_flag>);
// ... while std::atomic<T> is. Same header, same guarantee, opposite answer.
static_assert(is_synchronizable_v<std::atomic<int>>);

// So none of them can be handed to a second thread by reference.
static_assert(!is_sendable_v<std::mutex&>);
static_assert(!is_sendable_v<std::latch&>);
static_assert(!is_sendable_v<std::atomic_flag&>);
static_assert(is_sendable_v<std::atomic<int>&>);

// std::thread::id is a value type, trivially copyable, comparable, hashable --
// and is refused because libstdc++ stores it as a pointer to an opaque struct.
static_assert(!is_sendable_v<std::thread::id>);
static_assert(!is_lifetime_aware_v<std::thread::id>);
static_assert(!is_synchronizable_v<const std::thread::id>);
static_assert(!is_sendable_v<std::jthread>);
