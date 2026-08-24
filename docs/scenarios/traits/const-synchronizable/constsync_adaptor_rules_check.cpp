#include <threadsafe/threadsafe.h>

#include <bitset>
#include <queue>
#include <stack>
#include <vector>

namespace threadsafe {

template <class T, class C>
struct is_synchronizable<const std::stack<T, C>>
    : is_synchronizable<const C> {};

template <class T, class C>
struct is_synchronizable<const std::queue<T, C>>
    : is_synchronizable<const C> {};

template <class T, class C, class Cmp>
struct is_synchronizable<const std::priority_queue<T, C, Cmp>>
    : std::bool_constant<is_synchronizable_v<const C>
                         && is_synchronizable_v<const Cmp>> {};

template <std::size_t N>
struct is_synchronizable<const std::bitset<N>> : std::true_type {};

}

using threadsafe::is_synchronizable_v;

static_assert(is_synchronizable_v<const std::stack<int>>);
static_assert(is_synchronizable_v<const std::queue<int>>);
static_assert(is_synchronizable_v<const std::priority_queue<int>>);
static_assert(is_synchronizable_v<const std::bitset<64>>);
static_assert(!is_synchronizable_v<const std::stack<int *>>,
              "and they still propagate through the element type");
static_assert(!is_synchronizable_v<const std::queue<int *>>);
