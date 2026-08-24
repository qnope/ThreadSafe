#include <threadsafe/threadsafe.h>

#include <array>
#include <deque>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <list>
#include <map>
#include <mdspan>
#include <print>
#include <ranges>
#include <regex>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

template <class T>
void report(const char *label) {
    std::println("{:52} lifetime_aware={:5} sendable={:5} borrowed_range={}",
                 label, threadsafe::is_lifetime_aware_v<T>,
                 threadsafe::is_sendable_v<T>,
                 std::ranges::borrowed_range<T>);
}

using RefView = std::ranges::ref_view<std::vector<int>>;
using AllTOfLvalue = std::views::all_t<std::vector<int> &>;
using OwningView = std::ranges::owning_view<std::vector<int>>;
using TransformOverRef =
    decltype(std::declval<std::vector<int> &>()
             | std::views::transform([](int value) { return value; }));
using FilterOverRef =
    decltype(std::declval<std::vector<int> &>()
             | std::views::filter([](int value) { return value > 0; }));
using ZipOverRefs = decltype(std::views::zip(std::declval<std::vector<int> &>(),
                                             std::declval<std::vector<int> &>()));
using JoinOverRef =
    decltype(std::declval<std::vector<std::vector<int>> &>()
             | std::views::join);

int main() {
    report<std::initializer_list<int>>("std::initializer_list<int>");
    report<RefView>("std::ranges::ref_view<std::vector<int>>");
    report<AllTOfLvalue>("std::views::all_t<std::vector<int>&>");
    report<OwningView>("std::ranges::owning_view<std::vector<int>>");
    report<TransformOverRef>("transform_view over an lvalue vector");
    report<FilterOverRef>("filter_view over an lvalue vector");
    report<ZipOverRefs>("zip_view over two lvalue vectors");
    report<JoinOverRef>("join_view over an lvalue vector of vectors");
    report<std::mdspan<int, std::dextents<std::size_t, 2>>>("std::mdspan<int, dextents<size_t,2>>");
    report<std::vector<int>::iterator>("std::vector<int>::iterator");
    report<std::vector<int>::const_iterator>("std::vector<int>::const_iterator");
    report<std::list<int>::iterator>("std::list<int>::iterator");
    report<std::map<int, int>::iterator>("std::map<int,int>::iterator");
    report<std::deque<int>::iterator>("std::deque<int>::iterator");
    report<std::set<int>::iterator>("std::set<int>::iterator");
    report<std::string::iterator>("std::string::iterator");
    report<std::array<int, 4>::iterator>("std::array<int,4>::iterator");
    report<std::reverse_iterator<std::vector<int>::iterator>>("reverse_iterator<vector<int>::iterator>");
    report<std::back_insert_iterator<std::vector<int>>>("back_insert_iterator<vector<int>>");
    report<std::smatch>("std::smatch");
    report<std::ssub_match>("std::ssub_match");
    report<std::sregex_iterator>("std::sregex_iterator");
    report<std::regex>("std::regex");
    return 0;
}
