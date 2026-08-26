#include <threadsafe/threadsafe.h>
#include <meta>
#include <type_traits>
namespace { struct WithZero { int z[0]; int v; }; }

static_assert(std::is_array_v<int[0]>);
static_assert(std::rank_v<int[0]> == 1);
static_assert(std::extent_v<int[0]> == 0);
static_assert(std::is_same_v<std::remove_extent_t<int[0]>, int>);
static_assert(std::meta::is_array_type(^^int[0]));
static_assert(std::meta::remove_extent(^^int[0]) == ^^int);
static_assert(threadsafe::is_sendable_type(^^int));
static_assert(!threadsafe::is_sendable_v<int[0]>, "the library says NO");
static_assert(!threadsafe::detail::default_is_sendable(^^int[0]),
              "even the primary walk says no?");
