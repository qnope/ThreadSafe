#include <threadsafe/threadsafe.h>
#include <string_view>
#include <span>
#include <vector>
#include <string>

using threadsafe::is_sendable_v;
using threadsafe::is_lifetime_aware_v;
using threadsafe::is_synchronizable_v;
using sync_int = threadsafe::synchronized_value<int>;

// which candidate borrow types are sendable?
static_assert(!is_sendable_v<std::string_view>  || true);
constexpr bool sv_sendable   = is_sendable_v<std::string_view>;
constexpr bool span_sendable = is_sendable_v<std::span<int>>;
constexpr bool ptr_sendable  = is_sendable_v<int*>;
constexpr bool syncptr_sendable = is_sendable_v<sync_int*>;
constexpr bool syncref_sendable = is_sendable_v<sync_int&>;
constexpr bool refwrap_sendable = is_sendable_v<std::reference_wrapper<sync_int>>;

static_assert(sv_sendable   == false, "string_view sendable?");
static_assert(span_sendable == false, "span sendable?");
static_assert(ptr_sendable  == false, "int* sendable?");
static_assert(syncptr_sendable == true, "sync_int* sendable?");
static_assert(syncref_sendable == true, "sync_int& sendable?");
static_assert(refwrap_sendable == true, "refwrap sendable?");
