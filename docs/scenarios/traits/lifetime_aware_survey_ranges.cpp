#include <threadsafe/threadsafe.h>
#include <span>
#include <string_view>
#include <ranges>
#include <vector>
#include <string>

using threadsafe::is_lifetime_aware_v;
#define PROBE(NAME, ...) static_assert(is_lifetime_aware_v<__VA_ARGS__>, "FALSE: " NAME);

using Vec = std::vector<int>;
using FilterView = decltype(std::declval<Vec&>() | std::views::filter(+[](int x){ return x > 0; }));
using AllView    = decltype(std::views::all(std::declval<Vec&>()));
using RefView    = std::ranges::ref_view<Vec>;
using OwningView = std::ranges::owning_view<Vec>;
using IotaView   = decltype(std::views::iota(0, 10));
using SingleView = decltype(std::views::single(std::string("x")));
using TransformOverRef = decltype(std::declval<Vec&>() | std::views::transform(+[](int x){ return x; }));
using TakeOverOwning = decltype(std::views::all(Vec{}) | std::views::take(3));
using ReverseOverRef = decltype(std::declval<Vec&>() | std::views::reverse);
using JoinOverRef = decltype(std::declval<std::vector<std::vector<int>>&>() | std::views::join);
using SplitStr = decltype(std::declval<std::string&>() | std::views::split(' '));
using ElemView = std::ranges::empty_view<int>;
using RepeatView = decltype(std::views::repeat(std::string("y")));

PROBE("span<int>", std::span<int>)
PROBE("string_view", std::string_view)
PROBE("subrange<int*>", std::ranges::subrange<int*>)
PROBE("filter over lvalue vector", FilterView)
PROBE("views::all(vec) = ref_view", AllView)
PROBE("ref_view<Vec>", RefView)
PROBE("owning_view<Vec>", OwningView)
PROBE("iota_view", IotaView)
PROBE("single_view<string>", SingleView)
PROBE("transform over ref_view", TransformOverRef)
PROBE("take over owning_view", TakeOverOwning)
PROBE("reverse over ref_view", ReverseOverRef)
PROBE("join over ref_view", JoinOverRef)
PROBE("split over string&", SplitStr)
PROBE("empty_view<int>", ElemView)
PROBE("repeat_view<string>", RepeatView)
int main(){}
