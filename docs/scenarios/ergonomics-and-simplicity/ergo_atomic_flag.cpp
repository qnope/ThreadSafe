#include <threadsafe/threadsafe.h>
#include <atomic>
#include <memory>
static_assert(threadsafe::is_sendable_v<std::shared_ptr<std::atomic<int>>>);
static_assert(!threadsafe::is_sendable_v<std::shared_ptr<std::atomic_flag>>);
static_assert(threadsafe::is_synchronizable_v<std::atomic<int>>);
static_assert(!threadsafe::is_synchronizable_v<std::atomic_flag>);
int main() {}
