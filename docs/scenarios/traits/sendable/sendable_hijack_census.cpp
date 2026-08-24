#include <threadsafe/threadsafe.h>

#include <bitset>
#include <chrono>
#include <complex>
#include <print>
#include <random>
#include <ratio>
#include <source_location>
#include <span>
#include <string_view>
#include <system_error>
#include <typeindex>
#include <utility>
#include <valarray>
#include <vector>

// Does may_hijack_copy_move (via has_only_default_copy_move_destroy) alone
// account for the rejection, or would the type be rejected anyway?
#define ROW(...)                                                               \
    std::println("{:<46} sendable={:<5} copy/move-guard-passed={}", #__VA_ARGS__,  \
                 threadsafe::is_sendable_v<__VA_ARGS__>,                       \
                 threadsafe::detail::has_only_default_copy_move_destroy(       \
                     ^^__VA_ARGS__))

int main() {
    ROW(std::chrono::nanoseconds);
    ROW(std::chrono::duration<double>);
    ROW(std::chrono::system_clock::time_point);
    ROW(std::chrono::year_month_day);
    ROW(std::chrono::hh_mm_ss<std::chrono::seconds>);
    ROW(std::complex<float>);
    ROW(std::bitset<8>);
    ROW(std::mt19937_64);
    ROW(std::uniform_int_distribution<int>);
    ROW(std::normal_distribution<double>);
    ROW(std::valarray<int>);
    ROW(std::error_code);
    ROW(std::type_index);
    ROW(std::string_view);
    ROW(std::span<int>);
    ROW(std::source_location);
    ROW(std::initializer_list<int>);
    ROW(std::in_place_t);
    ROW(std::ratio<1, 3>);
}
