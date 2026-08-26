#include <threadsafe/threadsafe.h>

#include <any>
#include <bitset>
#include <chrono>
#include <complex>
#include <expected>
#include <filesystem>
#include <flat_map>
#include <functional>
#include <initializer_list>
#include <mdspan>
#include <queue>
#include <span>
#include <stack>
#include <string_view>
#include <valarray>

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

#define P(NAME, ...) static_assert(__VA_ARGS__, "PROBE " NAME)

P("S span<int>",            is_sendable_v<std::span<int>>);
P("S string_view",          is_sendable_v<std::string_view>);
P("S bitset<8>",            is_sendable_v<std::bitset<8>>);
P("S valarray<int>",        is_sendable_v<std::valarray<int>>);
P("S initializer_list<int>",is_sendable_v<std::initializer_list<int>>);
P("S expected<int,int>",    is_sendable_v<std::expected<int,int>>);
P("S expected<int*,int>",   is_sendable_v<std::expected<int*,int>>);
P("S flat_map<int,int>",    is_sendable_v<std::flat_map<int,int>>);
P("S flat_map<int,int*>",   is_sendable_v<std::flat_map<int,int*>>);
P("S mdspan",               is_sendable_v<std::mdspan<int, std::extents<int,3>>>);
P("S function<void()>",     is_sendable_v<std::function<void()>>);
P("S move_only_function",   is_sendable_v<std::move_only_function<void()>>);
P("S any",                  is_sendable_v<std::any>);
P("S complex<double>",      is_sendable_v<std::complex<double>>);
P("S chrono::milliseconds", is_sendable_v<std::chrono::milliseconds>);
P("S fs::path",             is_sendable_v<std::filesystem::path>);
P("S stack<int>",           is_sendable_v<std::stack<int>>);
P("S queue<int>",           is_sendable_v<std::queue<int>>);
P("S priority_queue<int>",  is_sendable_v<std::priority_queue<int>>);
P("S stack<int*>",          is_sendable_v<std::stack<int*>>);
P("S queue<int*>",          is_sendable_v<std::queue<int*>>);
P("S priority_queue<int*>", is_sendable_v<std::priority_queue<int*>>);

P("L span<int>",            is_lifetime_aware_v<std::span<int>>);
P("L initializer_list<int>",is_lifetime_aware_v<std::initializer_list<int>>);
P("L bitset<8>",            is_lifetime_aware_v<std::bitset<8>>);
P("L valarray<int>",        is_lifetime_aware_v<std::valarray<int>>);
P("L expected<int,int>",    is_lifetime_aware_v<std::expected<int,int>>);
P("L flat_map<int,int>",    is_lifetime_aware_v<std::flat_map<int,int>>);
P("L mdspan",               is_lifetime_aware_v<std::mdspan<int, std::extents<int,3>>>);
P("L function<void()>",     is_lifetime_aware_v<std::function<void()>>);
P("L any",                  is_lifetime_aware_v<std::any>);
P("L fs::path",             is_lifetime_aware_v<std::filesystem::path>);
P("L stack<int>",           is_lifetime_aware_v<std::stack<int>>);
P("L stack<int*>",          is_lifetime_aware_v<std::stack<int*>>);
P("L queue<int>",           is_lifetime_aware_v<std::queue<int>>);
P("L priority_queue<int>",  is_lifetime_aware_v<std::priority_queue<int>>);
P("L chrono::milliseconds", is_lifetime_aware_v<std::chrono::milliseconds>);
P("L complex<double>",      is_lifetime_aware_v<std::complex<double>>);

P("CS const span<int>",           is_synchronizable_v<const std::span<int>>);
P("CS const string_view",         is_synchronizable_v<const std::string_view>);
P("CS const bitset<8>",           is_synchronizable_v<const std::bitset<8>>);
P("CS const valarray<int>",       is_synchronizable_v<const std::valarray<int>>);
P("CS const init_list<int>",      is_synchronizable_v<const std::initializer_list<int>>);
P("CS const expected<int,int>",   is_synchronizable_v<const std::expected<int,int>>);
P("CS const flat_map<int,int>",   is_synchronizable_v<const std::flat_map<int,int>>);
P("CS const function<void()>",    is_synchronizable_v<const std::function<void()>>);
P("CS const any",                 is_synchronizable_v<const std::any>);
P("CS const complex<double>",     is_synchronizable_v<const std::complex<double>>);
P("CS const chrono::ms",          is_synchronizable_v<const std::chrono::milliseconds>);
P("CS const fs::path",            is_synchronizable_v<const std::filesystem::path>);
P("CS const stack<int>",          is_synchronizable_v<const std::stack<int>>);
P("CS const stack<int*>",         is_synchronizable_v<const std::stack<int*>>);
P("CS const queue<int>",          is_synchronizable_v<const std::queue<int>>);
P("CS const priority_queue<int>", is_synchronizable_v<const std::priority_queue<int>>);
P("CS const mdspan",              is_synchronizable_v<const std::mdspan<int, std::extents<int,3>>>);

int main() {}
