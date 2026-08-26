#include <threadsafe/threadsafe.h>
#include <functional>
#include <span>
#include <vector>

namespace { struct SyncType { int v; }; }
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);
void free_function() {}
void run(std::reference_wrapper<void()> f) { f(); }
void touch(std::span<SyncType> s) { for (auto &e : s) e.v = 1; }

using threadsafe::is_sendable_v;
using threadsafe::is_lifetime_aware_v;
using threadsafe::is_synchronizable_v;

static_assert(is_lifetime_aware_v<void (*)()>);
static_assert(is_lifetime_aware_v<std::reference_wrapper<void()>>);
static_assert(!is_lifetime_aware_v<std::reference_wrapper<int>>);
static_assert(is_sendable_v<std::reference_wrapper<void()>>);

static_assert(is_sendable_v<SyncType*>);
static_assert(is_sendable_v<std::span<SyncType>>);
static_assert(!is_sendable_v<std::span<int>>);
static_assert(!is_sendable_v<std::span<const int>>);
static_assert(!is_lifetime_aware_v<std::span<SyncType>>);
static_assert(is_synchronizable_v<const std::span<SyncType>>);
static_assert(!is_synchronizable_v<const std::span<int>>);

int main() {
    std::vector<SyncType> data(4);
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(&run, std::ref(free_function));
    launcher.launch_scoped_task(&touch, std::span<SyncType>{data});
}
