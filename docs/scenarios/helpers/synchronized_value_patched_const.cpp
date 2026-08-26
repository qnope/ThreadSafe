#include <threadsafe/threadsafe.h>
#include <vector>
using sync_vec = threadsafe::synchronized_value<std::vector<int>>;
template <class T> constexpr bool can_with = requires(T v) { v.with([](std::vector<int>&){}); };
template <class T> constexpr bool can_with_shared = requires(T v) { v.with_shared([](const std::vector<int>&){}); };
static_assert(can_with<sync_vec&> && can_with_shared<sync_vec&>);
static_assert(!can_with<const sync_vec&>, "const offers readers only");
static_assert(can_with_shared<const sync_vec&>);
