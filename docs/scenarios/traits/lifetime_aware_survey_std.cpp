#include <threadsafe/threadsafe.h>
#include <functional>
#include <any>
#include <coroutine>
#include <stop_token>
#include <thread>
#include <future>
#include <filesystem>
#include <bitset>
#include <chrono>
#include <complex>
#include <atomic>
#include <mutex>
#include <string>
#include <memory>
#include <memory_resource>
#include <expected>
#include <initializer_list>
#include <valarray>
#include <regex>
#include <random>

using threadsafe::is_lifetime_aware_v;
#define PROBE(NAME, ...) static_assert(is_lifetime_aware_v<__VA_ARGS__>, "FALSE: " NAME);

struct Promise { std::suspend_always initial_suspend(); std::suspend_always final_suspend() noexcept; void return_void(); void unhandled_exception(); struct H { using promise_type = Promise; }; H get_return_object(); };

PROBE("std::function<void()>", std::function<void()>)
PROBE("std::move_only_function<void()>", std::move_only_function<void()>)
PROBE("std::any", std::any)
PROBE("std::coroutine_handle<>", std::coroutine_handle<>)
PROBE("std::coroutine_handle<Promise>", std::coroutine_handle<Promise>)
PROBE("std::thread", std::thread)
PROBE("std::jthread", std::jthread)
PROBE("std::future<int>", std::future<int>)
PROBE("std::shared_future<int>", std::shared_future<int>)
PROBE("std::promise<int>", std::promise<int>)
PROBE("std::packaged_task<int()>", std::packaged_task<int()>)
PROBE("std::filesystem::path", std::filesystem::path)
PROBE("std::bitset<64>", std::bitset<64>)
PROBE("chrono::nanoseconds", std::chrono::nanoseconds)
PROBE("chrono::steady_clock::time_point", std::chrono::steady_clock::time_point)
PROBE("std::complex<double>", std::complex<double>)
PROBE("std::atomic<int>", std::atomic<int>)
PROBE("std::mutex", std::mutex)
PROBE("std::expected<int,std::string>", std::expected<int,std::string>)
PROBE("std::initializer_list<int>", std::initializer_list<int>)
PROBE("std::valarray<int>", std::valarray<int>)
PROBE("std::regex", std::regex)
PROBE("std::mt19937", std::mt19937)
PROBE("std::pmr::string", std::pmr::string)
PROBE("std::error_code", std::error_code)
PROBE("std::exception_ptr", std::exception_ptr)
int main(){}
