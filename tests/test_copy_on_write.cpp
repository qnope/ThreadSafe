#include <threadsafe/copy_on_write.h>

#include <concepts>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <threadsafe/asynchronous_task_launcher.h>

namespace {
struct NonSendable {
    NonSendable(NonSendable const&) {}
};

struct Cache {
    int raw;
    mutable std::optional<int> parsed;
};

struct Outer {
    Cache cache;
};

struct Arrayed {
    Cache cache[4];
};

struct SyncCache {
    mutable std::optional<int> parsed;
};

template <class T>
struct Tagged {
    int value;
};

template <class F, class... Args>
constexpr bool can_launch_task =
    requires(threadsafe::asynchronous_task_launcher l, F f, Args... args) {
        l.launch_task(f, args...);
    };

template <class C>
constexpr bool can_detach = requires(C c) { c.as_mutable(); };

template <class T>
using cow = threadsafe::copy_on_write<T>;
}

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncCache);

using threadsafe::is_lifetime_aware;
using threadsafe::is_sendable;
using threadsafe::is_synchronizable;

static_assert(is_sendable<cow<int>>,
              "is_sendable — readers only ever see a const T, and a writer "
              "detaches before touching a shared one");
static_assert(is_sendable<cow<std::string>> && is_sendable<cow<std::vector<int>>>
                  && is_sendable<cow<std::map<int, std::string>>>
                  && is_sendable<cow<std::unordered_map<int, std::string>>>,
              "is_sendable — also a regression check on libstdc++ internals: a "
              "mutable member in one of them would silently demand Sync");
static_assert(!is_sendable<cow<NonSendable>>,
              "is_sendable — the T is copied on the receiving thread and "
              "destroyed by whoever drops the last handle");

static_assert(!is_sendable<cow<Cache>>,
              "is_sendable — a const method that writes turns two concurrent "
              "readers into a data race the detach never sees");
static_assert(!is_sendable<cow<Outer>>,
              "is_sendable — mutable state is looked for through members");
static_assert(!is_sendable<cow<Arrayed>>,
              "is_sendable — and through arrays");
static_assert(!is_sendable<cow<std::vector<Cache>>>
                  && !is_sendable<cow<std::optional<Cache>>>,
              "is_sendable — a standard container hides its elements behind "
              "pointers, so they are read off its template arguments");
static_assert(is_sendable<cow<Tagged<Cache>>>,
              "is_sendable — that template-argument reading stops at namespace "
              "std: elsewhere an argument says nothing about what is held");
static_assert(is_sendable<cow<SyncCache>>,
              "is_sendable — a T that handles its own concurrent access needs "
              "no help from the copy-on-write discipline");

static_assert(!is_synchronizable<cow<int>>,
              "is_synchronizable — as_mutable rebinds the handle, so one "
              "copy_on_write object belongs to one thread; share by copying it");

static_assert(is_lifetime_aware<cow<std::string>>,
              "is_lifetime_aware — the shared block keeps the T alive");
static_assert(!is_lifetime_aware<cow<int*>>,
              "is_lifetime_aware — ownership is transitive, an owned borrow is "
              "still a borrow");

static_assert(std::same_as<decltype(*std::declval<cow<int>&>()), const int&>,
              "a non-const handle still hands out a const reference — the only "
              "way to write is as_mutable");
static_assert(
    std::same_as<decltype(std::declval<cow<int>&>().operator->()), const int*>);
static_assert(
    std::same_as<decltype(std::declval<cow<int>&>().as_mutable()), int&>);

static_assert(std::copy_constructible<cow<int>>
                  && std::constructible_from<cow<int>, cow<int>&>,
              "the variadic constructor must not outrank the copy constructor "
              "on a non-const lvalue, or the type stops being a COW");

static_assert(can_detach<cow<int>>);
static_assert(!can_detach<cow<std::unique_ptr<int>>>,
              "as_mutable — a T that cannot be copied gives a read-only "
              "handle, not a hard error");

static_assert(can_launch_task<decltype([](cow<std::string>) {}),
                              cow<std::string>>,
              "launch_task — the point of the type: a shared, unsynchronized "
              "read that costs no lock and no eager copy");
static_assert(!can_launch_task<decltype([](cow<Cache>) {}), cow<Cache>>);
