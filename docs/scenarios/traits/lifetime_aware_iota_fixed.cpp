#include <threadsafe/threadsafe.h>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>
using threadsafe::is_lifetime_aware_v;
#define YES(N,...) static_assert(is_lifetime_aware_v<__VA_ARGS__>, "GOT-FALSE: " N);
#define NO(N,...)  static_assert(!is_lifetime_aware_v<__VA_ARGS__>, "GOT-TRUE: " N);
int a[4];
YES("iota(0,10)", decltype(std::views::iota(0,10)))
YES("iota(0)", decltype(std::views::iota(0)))
NO("iota over pointers", decltype(std::views::iota(a+0, a+4)))
NO("span<int>", std::span<int>)
NO("string_view", std::string_view)
NO("subrange<int*>", std::ranges::subrange<int*>)
NO("ref_view<vector<int>>", std::ranges::ref_view<std::vector<int>>)
NO("empty_view<int>", std::ranges::empty_view<int>)
int main(){}
