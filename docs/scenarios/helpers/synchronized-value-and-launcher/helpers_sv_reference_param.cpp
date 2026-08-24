#include <threadsafe/threadsafe.h>
#include <atomic>
using sync_ref = threadsafe::synchronized_value<std::atomic<int>&>;
static_assert(threadsafe::is_synchronizable_v<sync_ref>);
static_assert(threadsafe::is_sendable_v<sync_ref>);
static_assert(!threadsafe::is_lifetime_aware_v<sync_ref>);
int main() {
    std::atomic<int> counter{0};
    sync_ref borrowed{counter};
    auto locked = borrowed.lock();
    (*locked).fetch_add(1);
}
