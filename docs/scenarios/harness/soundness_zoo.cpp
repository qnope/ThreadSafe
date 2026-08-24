#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <deque>
#include <list>
#include <map>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

namespace {
template <class T>
void row(const char *label) {
    std::printf("%-58s borrowed=%d lifetime_aware=%d\n", label,
                (int)std::ranges::borrowed_range<T>,
                (int)threadsafe::is_lifetime_aware_v<T>);
}
using V = std::vector<int>;
using M = std::map<int,int>;
}

int main() {
    row<std::ranges::subrange<std::list<int>::iterator>>("subrange<list<int>::iterator>");
    row<std::ranges::subrange<std::deque<int>::iterator>>("subrange<deque<int>::iterator>");
    row<std::ranges::subrange<M::iterator>>("subrange<map::iterator>");
    row<std::ranges::take_view<std::ranges::ref_view<V>>>("take_view<ref_view<vector>>");
    row<std::ranges::drop_view<std::ranges::ref_view<V>>>("drop_view<ref_view<vector>>");
    row<std::ranges::reverse_view<std::ranges::ref_view<V>>>("reverse_view<ref_view<vector>>");
    row<std::ranges::common_view<std::ranges::subrange<int*, std::unreachable_sentinel_t>>>("common_view<subrange<int*,unreachable>>");
    row<std::ranges::zip_view<std::ranges::ref_view<V>, std::ranges::ref_view<V>>>("zip_view<ref_view,ref_view>");
    row<std::ranges::elements_view<std::ranges::ref_view<std::vector<std::pair<int,int>>>,0>>("elements_view<ref_view<vector<pair>>,0>");
    row<std::ranges::as_const_view<std::ranges::ref_view<V>>>("as_const_view<ref_view<vector>>");
    row<std::ranges::as_rvalue_view<std::ranges::ref_view<V>>>("as_rvalue_view<ref_view<vector>>");
    row<std::ranges::enumerate_view<std::ranges::ref_view<V>>>("enumerate_view<ref_view<vector>>");
    row<std::ranges::stride_view<std::ranges::ref_view<V>>>("stride_view<ref_view<vector>>");
    row<std::ranges::chunk_view<std::ranges::ref_view<V>>>("chunk_view<ref_view<vector>>");
    row<std::ranges::slide_view<std::ranges::ref_view<V>>>("slide_view<ref_view<vector>>");
    row<std::ranges::cartesian_product_view<std::ranges::ref_view<V>, std::ranges::ref_view<V>>>("cartesian_product_view<ref_view,ref_view>");
    row<std::ranges::owning_view<V>>("owning_view<vector<int>>  (owns; expect aware)");
    row<std::ranges::single_view<int>>("single_view<int>  (owns; expect aware)");
    row<std::ranges::repeat_view<int>>("repeat_view<int>  (owns; expect aware)");
    return 0;
}
