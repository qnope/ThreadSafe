#include <threadsafe/threadsafe.h>

#include <any>
#include <bitset>
#include <chrono>
#include <complex>
#include <coroutine>
#include <cstdio>
#include <exception>
#include <expected>
#include <filesystem>
#include <functional>
#include <future>
#include <iostream>
#include <latch>
#include <barrier>
#include <locale>
#include <mutex>
#include <condition_variable>
#include <random>
#include <semaphore>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <typeindex>
#include <valarray>
#include <vector>
#include <print>

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

#define ROW(...)                                                              \
    std::println("{:<44} sendable={:<5} sync<const T>={}", #__VA_ARGS__,      \
                 is_sendable_v<__VA_ARGS__>,                                  \
                 is_synchronizable_v<const __VA_ARGS__>)

int main() {
    ROW(int);
    ROW(std::string);
    ROW(std::vector<int>);
    ROW(std::chrono::milliseconds);
    ROW(std::chrono::steady_clock::time_point);
    ROW(std::chrono::system_clock::duration);
    ROW(std::complex<double>);
    ROW(std::bitset<64>);
    ROW(std::error_code);
    ROW(std::error_condition);
    ROW(std::type_index);
    ROW(std::exception_ptr);
    ROW(std::any);
    ROW(std::coroutine_handle<>);
    ROW(std::locale);
    ROW(std::thread);
    ROW(std::jthread);
    ROW(std::future<int>);
    ROW(std::promise<int>);
    ROW(std::packaged_task<int()>);
    ROW(std::latch);
    ROW(std::barrier<>);
    ROW(std::counting_semaphore<>);
    ROW(std::condition_variable);
    ROW(std::mutex);
    ROW(std::shared_mutex);
    ROW(std::filesystem::path);
    ROW(std::random_device);
    ROW(std::mt19937);
    ROW(FILE*);
    ROW(std::ostream*);
    ROW(std::string_view);
    ROW(std::span<int>);
    ROW(std::expected<int, std::string>);
    ROW(std::valarray<int>);
    ROW(std::function<void()>);
    ROW(std::move_only_function<void()>);
    ROW(std::vector<std::chrono::milliseconds>);
    ROW(std::pair<int, std::chrono::seconds>);
    ROW(std::atomic<int>);
    ROW(std::atomic_flag);
    ROW(std::stop_token);
    ROW(std::stop_source);
    ROW(std::nullptr_t);
    ROW(std::monostate);
}
