#include <threadsafe/threadsafe.h>
#include <concepts>
#include <memory>
#include <vector>

using sync_vec = threadsafe::synchronized_value<std::vector<int>>;

template <class T> constexpr bool can_lock = requires(T v) { v.lock(); };
template <class T> constexpr bool can_lock_shared = requires(T v) { v.lock_shared(); };

static_assert(can_lock<sync_vec&> && can_lock_shared<sync_vec&>);
static_assert(!can_lock<const sync_vec&>, "const offers readers only");
static_assert(can_lock_shared<const sync_vec&>);

static_assert(threadsafe::is_synchronizable_v<sync_vec>);
static_assert(threadsafe::is_synchronizable_v<const sync_vec>);
static_assert(threadsafe::is_sendable_v<std::shared_ptr<sync_vec>>
              && threadsafe::is_lifetime_aware_v<std::shared_ptr<sync_vec>>);

// A non-movable guard can still be returned as a prvalue.
sync_vec::guard borrow(sync_vec& value) { return value.lock(); }

int main() {
    sync_vec value{};
    const auto guard = borrow(value);
    guard->push_back(1);
}
