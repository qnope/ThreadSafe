#include <type_traits>
#include <meta>
static_assert(std::is_array_v<int[0]>, "std::is_array_v<int[0]> is FALSE");
static_assert(std::rank_v<int[0]> == 1, "rank not 1");
static_assert(std::extent_v<int[0]> == 0, "extent not 0");
static_assert(!std::meta::is_array_type(^^int[0]), "meta::is_array_type IS true");
