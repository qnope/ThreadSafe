#include <threadsafe/threadsafe.h>
#include <any>
#include <atomic>
#include <bitset>
#include <chrono>
#include <complex>
#include <coroutine>
#include <exception>
#include <filesystem>
#include <format>
#include <functional>
#include <future>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <typeindex>
#include <valarray>
#include <variant>
#include <vector>
#include <map>
#include <unordered_map>
#include <array>
#include <cstdio>
using threadsafe::is_sendable_v; using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;
template <class T> void row(const char* name) {
    std::printf("| `%-42s` | %-3s | %-3s | %-3s | %-3s |\n", name,
        is_sendable_v<T> ? "yes" : "no", is_synchronizable_v<T> ? "yes" : "no",
        is_synchronizable_v<const T> ? "yes" : "no", is_lifetime_aware_v<T> ? "yes" : "no");
}
int main() {
    std::printf("| type | sendable | sync<T> | sync<const T> | lifetime |\n");
    std::printf("|---|---|---|---|---|\n");
    row<int>("int");
    row<const int>("const int");
    row<int*>("int*");
    row<const int*>("const int*");
    row<int* const>("int* const");
    row<int&>("int&");
    row<const int&>("const int&");
    row<int[4]>("int[4]");
    row<void()>("void()");
    row<void(*)()>("void(*)()");
    row<std::atomic<int>>("std::atomic<int>");
    row<std::atomic<int*>>("std::atomic<int*>");
    row<std::atomic<std::shared_ptr<int>>>("std::atomic<std::shared_ptr<int>>");
    row<std::mutex>("std::mutex");
    row<std::shared_mutex>("std::shared_mutex");
    row<std::thread>("std::thread");
    row<std::jthread>("std::jthread");
    row<std::stop_token>("std::stop_token");
    row<std::string>("std::string");
    row<std::string_view>("std::string_view");
    row<std::vector<int>>("std::vector<int>");
    row<std::vector<int*>>("std::vector<int*>");
    row<std::span<int>>("std::span<int>");
    row<std::vector<int>::iterator>("std::vector<int>::iterator");
    row<std::initializer_list<int>>("std::initializer_list<int>");
    row<std::ranges::empty_view<int>>("std::ranges::empty_view<int>");
    row<std::unique_ptr<int>>("std::unique_ptr<int>");
    row<std::shared_ptr<int>>("std::shared_ptr<int>");
    row<std::weak_ptr<int>>("std::weak_ptr<int>");
    row<std::reference_wrapper<int>>("std::reference_wrapper<int>");
    row<std::function<void()>>("std::function<void()>");
    row<std::move_only_function<void()>>("std::move_only_function<void()>");
    row<std::any>("std::any");
    row<std::exception_ptr>("std::exception_ptr");
    row<std::error_code>("std::error_code");
    row<std::type_index>("std::type_index");
    row<std::filesystem::path>("std::filesystem::path");
    row<std::optional<int>>("std::optional<int>");
    row<std::pair<int,int*>>("std::pair<int,int*>");
    row<std::tuple<int,double>>("std::tuple<int,double>");
    row<std::variant<int,std::string>>("std::variant<int,std::string>");
    row<std::array<int,4>>("std::array<int,4>");
    row<std::map<int,std::string>>("std::map<int,std::string>");
    row<std::unordered_map<int,int>>("std::unordered_map<int,int>");
    row<std::future<int>>("std::future<int>");
    row<std::promise<int>>("std::promise<int>");
    row<std::coroutine_handle<>>("std::coroutine_handle<>");
    row<std::bitset<8>>("std::bitset<8>");
    row<std::valarray<int>>("std::valarray<int>");
    row<std::complex<double>>("std::complex<double>");
    row<std::chrono::system_clock::time_point>("std::chrono::system_clock::time_point");
}
