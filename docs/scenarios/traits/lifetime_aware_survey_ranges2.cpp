#include <threadsafe/threadsafe.h>
#include <ranges>
#include <vector>
#include <string>

using threadsafe::is_lifetime_aware_v;
#define PROBE(NAME, ...) static_assert(is_lifetime_aware_v<__VA_ARGS__>, "FALSE: " NAME);
using Vec = std::vector<int>;
inline bool positive(int x) { return x > 0; }

using FilterOverOwning  = decltype(std::views::all(Vec{}) | std::views::filter(&positive));
using DropWhileOwning   = decltype(std::views::all(Vec{}) | std::views::drop_while(&positive));
using TransformOwning   = decltype(std::views::all(Vec{}) | std::views::transform(&positive));
using ReverseOwning     = decltype(std::views::all(Vec{}) | std::views::reverse);
using JoinOwning        = decltype(std::views::all(std::vector<std::string>{}) | std::views::join);
using ChunkOwning       = decltype(std::views::all(Vec{}) | std::views::chunk(2));
using SplitOwning       = decltype(std::views::all(std::string{}) | std::views::split(' '));
using IotaUnbounded     = decltype(std::views::iota(0));

PROBE("filter over owning_view", FilterOverOwning)
PROBE("drop_while over owning_view", DropWhileOwning)
PROBE("transform over owning_view", TransformOwning)
PROBE("reverse over owning_view", ReverseOwning)
PROBE("join over owning_view", JoinOwning)
PROBE("chunk over owning_view", ChunkOwning)
PROBE("split over owning_view", SplitOwning)
PROBE("iota unbounded", IotaUnbounded)
int main(){}
