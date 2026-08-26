#include <threadsafe/threadsafe.h>

#include <atomic>

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

// A reference to a std::atomic<int> may be shared and sent.
static_assert(is_synchronizable_v<std::atomic<int>>);
static_assert(is_sendable_v<std::atomic<int>&>);

// std::atomic_ref<int> is the same thing spelled the other way round -- every
// access it makes to the referenced int is atomic -- yet it is neither.
static_assert(!is_synchronizable_v<std::atomic_ref<int>>);
static_assert(!is_sendable_v<std::atomic_ref<int>>);
static_assert(!is_synchronizable_v<const std::atomic_ref<int>>);
static_assert(!is_sendable_v<std::atomic_ref<int>&>);

// So the standard's only way to give an existing, plainly-declared object
// atomic access cannot cross a thread boundary at all.
static_assert(!threadsafe::launchable_scoped_task<
                  decltype([](std::atomic_ref<int>) {}),
                  std::atomic_ref<int>>);

int main() {}
