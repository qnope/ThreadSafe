#include <threadsafe/threadsafe.h>
#include <array>
#include <string>
#include <atomic>
namespace { struct Mut { int a; mutable int b; }; struct Vouched { mutable int c; }; }
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(std::array<Vouched, 2>);
using threadsafe::is_sendable_v; using threadsafe::is_synchronizable_v; using threadsafe::is_lifetime_aware_v;
#define P(...) static_assert((__VA_ARGS__));
#define N(...) static_assert(!(__VA_ARGS__));
P(is_sendable_v<std::array<int,3>>)
P(is_sendable_v<std::array<std::string,3>>)
N(is_sendable_v<std::array<int*,3>>)
P(is_sendable_v<std::array<int,0>>)
P(is_synchronizable_v<const std::array<int,3>>)
N(is_synchronizable_v<const std::array<Mut,3>>)
N(is_synchronizable_v<const std::array<int*,3>>)
P(is_synchronizable_v<const std::array<int,0>>)
P(is_lifetime_aware_v<std::array<std::string,3>>)
N(is_lifetime_aware_v<std::array<int*,3>>)
P(is_lifetime_aware_v<std::array<int,0>>)
P(is_sendable_v<std::array<Vouched,2>>)
P(is_synchronizable_v<const std::array<Vouched,2>>)
P(is_sendable_v<std::array<std::array<std::string,2>,2>>)
N(is_sendable_v<std::array<std::array<int*,2>,2>>)
int main(){}
