#include <threadsafe/threadsafe.h>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <vector>

using threadsafe::copy_on_write;
template <class T> using cow = copy_on_write<T>;

// copy / move
static_assert(std::is_copy_constructible_v<cow<int>>);
static_assert(std::is_move_constructible_v<cow<int>>);
static_assert(std::is_copy_assignable_v<cow<int>>);
static_assert(std::is_move_assignable_v<cow<int>>);
static_assert(std::constructible_from<cow<int>, cow<int>&>,
              "the guard defeats the greedy template on a NON-const lvalue");
static_assert(std::constructible_from<cow<int>, const cow<int>&>);
static_assert(std::constructible_from<cow<int>, cow<int>&&>);

// default
static_assert(std::is_default_constructible_v<cow<int>>);
static_assert(!std::is_default_constructible_v<cow<std::vector<int>>&>);
static_assert(std::is_default_constructible_v<cow<std::vector<int>>>);

// explicit
static_assert(!std::is_convertible_v<int, cow<int>>, "constructor is explicit");

// from a cow of a DIFFERENT T -- the guard only names copy_on_write<T> itself,
// so cow<long> from cow<int>& goes through the greedy template if long is
// constructible from cow<int>. It is not, so this is simply rejected:
static_assert(!std::constructible_from<cow<long>, cow<int>&>);

// ... but a T that swallows anything makes the greedy template fire:
struct Swallow { template <class U> Swallow(U&&) {} };
static_assert(std::constructible_from<cow<Swallow>, cow<int>&>,
              "cow<Swallow> from a cow<int>: wraps the cow<int> inside a Swallow");

// initializer_list: the variadic ctor cannot deduce one
static_assert(std::constructible_from<cow<std::vector<int>>, std::initializer_list<int>>,
              "a NAMED initializer_list is forwarded fine");
static_assert(std::constructible_from<cow<std::vector<int>>, int, int>,
              "cow<vector<int>>(3, 0) -> vector(3, 0), i.e. three zeroes");
int main() {
    cow<std::vector<int>> braced{std::vector<int>{1, 2, 3}};
    static_assert(std::is_same_v<decltype(braced), cow<std::vector<int>>>);
    cow<std::vector<int>> counted{3, 0};
    (void)braced; (void)counted;
}
