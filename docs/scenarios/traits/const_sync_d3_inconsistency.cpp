#include <threadsafe/threadsafe.h>
#include <atomic>
static_assert(threadsafe::is_sendable_v<std::atomic<int> &>, "sendable ref FALSE");
static_assert(!threadsafe::is_synchronizable_v<std::atomic<int> &>, "sync ref TRUE");
static_assert(threadsafe::is_synchronizable_v<std::atomic<int>>, "sync value FALSE");
static_assert(threadsafe::is_synchronizable_v<std::atomic<int> *const>, "sync ptr FALSE");
