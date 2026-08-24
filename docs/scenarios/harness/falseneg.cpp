#include <threadsafe/threadsafe.h>
#include <bitset>
#include <chrono>
#include <complex>
#include <cstdio>
#include <filesystem>
#include <future>
#include <numeric>
#include <ratio>
#include <string>
#include <system_error>
#include <valarray>
#include <vector>
#include <bit>
#include <expected>
#include <utility>

using threadsafe::is_sendable_v;
// Every type below is a self-contained VALUE: it owns everything it holds and
// sharing nothing. A correct is_sendable should accept all of them.
#define ROW(...) std::printf("| `%-46s` | %-3s |\n", #__VA_ARGS__, is_sendable_v<__VA_ARGS__> ? "yes" : "NO")
int main() {
    std::printf("| self-contained value type | sendable |\n|---|---|\n");
    ROW(std::bitset<8>);
    ROW(std::complex<double>);
    ROW(std::chrono::milliseconds);
    ROW(std::chrono::system_clock::time_point);
    ROW(std::chrono::year_month_day);
    ROW(std::valarray<int>);
    ROW(std::error_code);
    ROW(std::error_condition);
    ROW(std::filesystem::path);
    ROW(std::filesystem::file_status);
    ROW(std::future<int>);
    ROW(std::promise<int>);
    ROW(std::expected<int, std::string>);
    ROW(std::string);
    ROW(std::vector<int>);
    ROW(std::pair<int, double>);
}
