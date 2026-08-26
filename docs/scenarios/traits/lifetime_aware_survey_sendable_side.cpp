#include <threadsafe/threadsafe.h>
#include <thread>
#include <future>
#include <valarray>
#include <filesystem>
#include <exception>
#include <functional>
using threadsafe::is_sendable_v;
#define PROBE(N,...) static_assert(is_sendable_v<__VA_ARGS__>, "NOT-SENDABLE: " N);
PROBE("std::thread", std::thread)
PROBE("std::jthread", std::jthread)
PROBE("std::future<int>", std::future<int>)
PROBE("std::valarray<int>", std::valarray<int>)
PROBE("std::filesystem::path", std::filesystem::path)
PROBE("std::exception_ptr", std::exception_ptr)
PROBE("std::function<void()>", std::function<void()>)
int main(){}
