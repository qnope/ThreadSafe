#include <threadsafe/threadsafe.h>

#include <atomic>
#include <concepts>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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

struct SafeCounter {
    mutable std::atomic<int> hits;
};

struct SelfRef {
    SelfRef* next;
};

struct PList {
    int v;
    const PList* tail;
};

template <class T>
struct Tagged {
    int value;
};

template <class F, class... Args>
constexpr bool can_launch_task = threadsafe::launchable_task<F, Args...>;

template <class C>
constexpr bool can_detach = requires(C c) { c.as_mutable(); };

template <class T>
using cow = threadsafe::copy_on_write<T>;
}

template <>
struct threadsafe::is_unsafe_synchronizable<SyncCache> {
    static consteval threadsafe::TraitAnswer diagnose() {
        return {};
    }
};

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(is_sendable_v<cow<int>>,
              "is_sendable — readers only ever see a const T, and a writer "
              "detaches before touching a shared one");
static_assert(is_sendable_v<cow<std::string>> && is_sendable_v<cow<std::vector<int>>>
                  && is_sendable_v<cow<std::map<int, std::string>>>
                  && is_sendable_v<cow<std::unordered_map<int, std::string>>>,
              "is_sendable — [res.on.data.races] makes a const standard "
              "container read-safe, which its explicit const rule now states");
static_assert(!is_sendable_v<cow<NonSendable>>,
              "is_sendable — the T is copied on the receiving thread and "
              "destroyed by whoever drops the last handle");

static_assert(!is_sendable_v<cow<Cache>>,
              "is_sendable — a const method that writes turns two concurrent "
              "readers into a data race the detach never sees");
static_assert(!is_sendable_v<cow<Outer>>,
              "is_sendable — the const recursion walks members");
static_assert(!is_sendable_v<cow<Arrayed>>,
              "is_sendable — and arrays");
static_assert(!is_sendable_v<cow<std::optional<Cache>>>,
              "is_sendable — a member held by value is walked into, whoever "
              "declares it");
static_assert(!is_sendable_v<cow<std::vector<Cache>>>,
              "is_sendable — an element is reached by every reader: the const "
              "rule follows the container's template arguments where the old "
              "mutable walk could not");
static_assert(is_sendable_v<cow<Tagged<Cache>>>,
              "is_sendable — a template argument of a user type is still not "
              "read as a member");
static_assert(is_sendable_v<cow<SyncCache>>,
              "is_sendable — a T that handles its own concurrent access needs "
              "no help from the copy-on-write discipline");
static_assert(is_sendable_v<cow<SafeCounter>>,
              "is_sendable — a mutable member whose own type handles the "
              "concurrency no longer costs the copy-on-write sendability");
static_assert(!is_sendable_v<cow<SelfRef>> && !is_sendable_v<cow<PList>>,
              "is_sendable — self-referential types answer, they do not recurse "
              "forever");

static_assert(!is_synchronizable_v<cow<int>>,
              "is_synchronizable — as_mutable rebinds the handle, so one "
              "copy_on_write object belongs to one thread; share by copying it");

static_assert(is_lifetime_aware_v<cow<std::string>>,
              "is_lifetime_aware — the shared block keeps the T alive");
static_assert(!is_lifetime_aware_v<cow<int*>>,
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
