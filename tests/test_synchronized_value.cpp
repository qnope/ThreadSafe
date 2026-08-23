#include <threadsafe/threadsafe.h>

#include <concepts>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>

namespace {
struct NonSendable {
    NonSendable(NonSendable const&) {}
};

struct Memo {
    int key;
    mutable int cached;
};

using sync_int = threadsafe::synchronized_value<int>;
using sync_memo = threadsafe::synchronized_value<Memo>;

template <class F, class... Args>
constexpr bool can_launch_task =
    requires(threadsafe::asynchronous_task_launcher l, F f, Args... args) {
        l.launch_task(f, args...);
    };

template <class F, class... Args>
constexpr bool can_launch_scoped_task =
    requires(threadsafe::asynchronous_task_launcher l, F f, Args... args) {
        l.launch_scoped_task(f, args...);
    };

template <class T>
constexpr bool can_lock = requires(T v) { v.lock(); };
template <class T>
constexpr bool can_lock_shared = requires(T v) { v.lock_shared(); };
}

using threadsafe::is_lifetime_aware;
using threadsafe::is_sendable;
using threadsafe::is_synchronizable;

static_assert(is_synchronizable<sync_int>,
              "is_synchronizable — the mutex serializes access, so a sendable "
              "T may be held by several threads at once");
static_assert(is_sendable<sync_int>,
              "is_sendable — Sync implies Send in this library");
static_assert(!is_synchronizable<threadsafe::synchronized_value<NonSendable>>,
              "is_synchronizable — the T still crosses threads one at a time, "
              "so a non-sendable T is not rescued by the mutex");

static_assert(is_lifetime_aware<threadsafe::synchronized_value<std::string>>,
              "is_lifetime_aware — the value is held by value; the mutex is "
              "not data and must not be recursed into");
static_assert(!is_lifetime_aware<threadsafe::synchronized_value<int*>>,
              "is_lifetime_aware — ownership is transitive, a guarded borrow "
              "is still a borrow");

static_assert(!is_sendable<sync_int::guard>,
              "is_sendable — a unique_lock must be released by the thread that "
              "took it");
static_assert(!is_sendable<sync_int::const_guard>);
static_assert(!is_lifetime_aware<sync_int::guard>,
              "is_lifetime_aware — the guard borrows the value, it does not "
              "keep it alive");
static_assert(!is_lifetime_aware<sync_int::const_guard>);

static_assert(is_sendable<std::shared_ptr<sync_int>>
                  && is_lifetime_aware<std::shared_ptr<sync_int>>,
              "a shared_ptr to a synchronized_value is the intended way to "
              "share a user type");
static_assert(can_launch_task<decltype([](std::shared_ptr<sync_int>) {}),
                              std::shared_ptr<sync_int>>,
              "launch_task — the whole point of the type: a checked path for "
              "sharing mutable state");

static_assert(!can_launch_task<decltype([](sync_int*) {}), sync_int*>,
              "launch_task — a raw pointer is sendable here, the pointee being "
              "synchronizable, but it keeps nothing alive");
static_assert(!can_launch_task<decltype([](sync_int::guard) {}),
                               sync_int::guard>,
              "launch_task — a guard does not cross a thread boundary");
static_assert(can_launch_scoped_task<decltype([](sync_int&) {}),
                                     std::reference_wrapper<sync_int>>,
              "launch_scoped_task — the launcher joins, so a reference to a "
              "synchronizable object may cross");

static_assert(!std::copy_constructible<sync_int::guard>
                  && !std::movable<sync_int::guard>,
              "a movable guard could be lodged in an aggregate and travel");
static_assert(!std::copy_constructible<sync_int>);

static_assert(
    std::same_as<decltype(*std::declval<const sync_int::guard&>()), int&>,
    "the guard hands out a mutable reference for the duration of the lock");
static_assert(std::same_as<decltype(*std::declval<const sync_int::const_guard&>()),
                           const int&>,
              "the shared guard hands out a const reference");
static_assert(std::same_as<decltype(sync_int::make(42)),
                           std::shared_ptr<sync_int>>,
              "the factory yields the sendable, lifetime-aware form");

static_assert(can_lock<sync_int&> && can_lock_shared<sync_int&>);
static_assert(!can_lock<const sync_int&>,
              "lock — a const synchronized_value grants readers only");
static_assert(can_lock_shared<const sync_int&>);

static_assert(std::same_as<sync_int::const_guard,
                           threadsafe::value_guard<
                               const int, std::shared_lock<std::shared_mutex>>>,
              "lock_shared — readers of a const-synchronizable T really share");
// A T whose const form is not synchronizable: `cached` is writable through a
// const&, so concurrent readers of a const Memo would race. The wrapper does
// not reject such a T, it downgrades the mutex to an exclusive one.
static_assert(!is_synchronizable<const Memo>,
              "the premise of the block below: a mutable member defeats const");
static_assert(is_sendable<Memo>,
              "one thread at a time is still fine, which is all the wrapper asks");

static_assert(std::same_as<sync_memo::mutex, std::mutex>,
              "no shared_mutex: there is no read that may be shared");
static_assert(std::same_as<sync_memo::guard,
                           threadsafe::value_guard<Memo,
                                                   std::unique_lock<std::mutex>>>);
static_assert(std::same_as<sync_memo::const_guard,
                           threadsafe::value_guard<const Memo,
                                                   std::unique_lock<std::mutex>>>,
              "lock_shared still hands out a unique_lock — readers are "
              "serialized against each other, not merely against writers");

static_assert(can_lock<sync_memo&> && can_lock_shared<sync_memo&>);
static_assert(!can_lock<const sync_memo&>,
              "a const wrapper grants readers only, whatever the mutex");
static_assert(can_lock_shared<const sync_memo&>,
              "the read is still offered; it is the exclusion that changed");
static_assert(
    std::same_as<decltype(*std::declval<const sync_memo::const_guard&>()),
                 const Memo&>);

static_assert(is_synchronizable<sync_memo>,
              "the wrapper is the fix: a T that no const& could share safely "
              "is shareable once every access goes through the mutex");
static_assert(is_synchronizable<const sync_memo>,
              "const only removes lock(), and lock_shared() is already safe");
static_assert(is_sendable<sync_memo>);
static_assert(is_lifetime_aware<sync_memo>,
              "the value is held by value, mutable member or not");

static_assert(!is_sendable<sync_memo::guard>
                  && !is_sendable<sync_memo::const_guard>,
              "a unique_lock must be released by the thread that took it — "
              "both guards hold one here");
static_assert(!is_lifetime_aware<sync_memo::guard>
                  && !is_lifetime_aware<sync_memo::const_guard>);

static_assert(can_launch_task<decltype([](std::shared_ptr<sync_memo>) {}),
                              std::shared_ptr<sync_memo>>,
              "sharing a const-unsafe T across threads is allowed once wrapped");
static_assert(!can_launch_task<decltype([](Memo&) {}),
                               std::reference_wrapper<Memo>>,
              "and refused when it is not");
