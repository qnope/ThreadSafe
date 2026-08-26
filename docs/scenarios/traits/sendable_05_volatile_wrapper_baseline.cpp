#include <threadsafe/threadsafe.h>
#include <vector>
using threadsafe::is_synchronizable_v;
using threadsafe::is_sendable_v;
using threadsafe::is_lifetime_aware_v;
static_assert(is_synchronizable_v<const std::vector<int>>);
static_assert(is_synchronizable_v<const volatile std::vector<int>>);
static_assert(is_lifetime_aware_v<const std::vector<int>>);
static_assert(is_lifetime_aware_v<volatile std::vector<int>>);
static_assert(is_sendable_v<volatile std::vector<int>>);
