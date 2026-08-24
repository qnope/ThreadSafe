#include <threadsafe/threadsafe.h>

#include <mdspan>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

using threadsafe::is_lifetime_aware_v;

static_assert(!is_lifetime_aware_v<std::span<int>>);
static_assert(!is_lifetime_aware_v<std::span<int, 4>>);
static_assert(!is_lifetime_aware_v<std::span<int, 0>>);
static_assert(!is_lifetime_aware_v<std::string_view>);
static_assert(!is_lifetime_aware_v<std::ranges::subrange<int *>>);
static_assert(!is_lifetime_aware_v<
              std::ranges::subrange<std::vector<int>::iterator>>);
static_assert(!is_lifetime_aware_v<std::ranges::ref_view<std::vector<int>>>);
static_assert(!is_lifetime_aware_v<std::views::all_t<std::vector<int> &>>);
static_assert(!is_lifetime_aware_v<decltype(std::views::zip(
                  std::declval<std::vector<int> &>(),
                  std::declval<std::vector<int> &>()))>);
static_assert(!is_lifetime_aware_v<std::mdspan<int, std::dextents<std::size_t, 2>>>);

static_assert(is_lifetime_aware_v<std::ranges::iota_view<int, int>>,
              "a computed range holds no one else's storage");
static_assert(is_lifetime_aware_v<std::ranges::empty_view<int>>,
              "an empty view holds nothing at all");

int main() { return 0; }
