#include <threadsafe/threadsafe.h>

#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>

namespace {
using sync_int = threadsafe::synchronized_value<int>;

// A guard bound to a temporary is destroyed at the semicolon; the rvalue
// overloads are deleted so it cannot hand out a reference that outlives it.
template <class V>
concept star_on_temporary = requires(V v) { *v.lock(); };
template <class V>
concept arrow_on_temporary = requires(V v) { v.lock().operator->(); };
template <class V>
concept star_on_temporary_shared = requires(const V v) { *v.lock_shared(); };
template <class V>
concept arrow_on_temporary_shared =
    requires(const V v) { v.lock_shared().operator->(); };
}

using threadsafe::is_lifetime_aware_v;

// --- is_lifetime_aware: the borrowed_range rule, on a type the structural
// walk cannot reject on its own. span/string_view/subrange all store a raw
// pointer, so the member walk already refuses them and they do not exercise
// the rule. An empty view has no member to walk.
static_assert(std::ranges::borrowed_range<std::ranges::empty_view<int>>
                  && std::is_empty_v<std::ranges::empty_view<int>>,
              "the premise: a borrowed range with no data member at all");
static_assert(!is_lifetime_aware_v<std::ranges::empty_view<int>>,
              "is_lifetime_aware — only the borrowed_range rule can reject an "
              "empty view; the member walk finds nothing to object to");
static_assert(!is_lifetime_aware_v<std::ranges::ref_view<std::string>>,
              "is_lifetime_aware — a ref_view borrows the range it wraps");

// --- value_guard: the deleted rvalue accessors. Nothing else pins this, and a
// regression would silently reopen the reference-escape hole.
static_assert(!star_on_temporary<sync_int>,
              "value_guard — operator* on a temporary guard must be deleted");
static_assert(!arrow_on_temporary<sync_int>,
              "value_guard — operator-> on a temporary guard must be deleted");
static_assert(!star_on_temporary_shared<sync_int>,
              "value_guard — likewise for the shared guard");
static_assert(!arrow_on_temporary_shared<sync_int>,
              "value_guard — likewise for the shared guard");
