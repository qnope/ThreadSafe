// What does the library say about the C++ features it will meet in 2026 code?
#include <threadsafe/threadsafe.h>

#include <chrono>
#include <expected>
#include <future>
#include <generator>
#include <print>
#include <span>
#include <string_view>

struct plain_aggregate {
    int value;
};

std::generator<int> counted(int limit) {
    for (int step = 0; step < limit; ++step)
        co_yield step;
}

int main() {
    std::println("std::chrono::milliseconds        sendable={}",
                 threadsafe::is_sendable_v<std::chrono::milliseconds>);
    std::println("std::chrono::steady_clock::time_point sendable={}",
                 threadsafe::is_sendable_v<std::chrono::steady_clock::time_point>);
    std::println("std::future<int>                 sendable={}",
                 threadsafe::is_sendable_v<std::future<int>>);
    std::println("std::promise<int>                sendable={}",
                 threadsafe::is_sendable_v<std::promise<int>>);
    std::println("std::expected<int, std::string>  sendable={}",
                 threadsafe::is_sendable_v<std::expected<int, std::string>>);
    std::println("std::generator<int>              sendable={}",
                 threadsafe::is_sendable_v<std::generator<int>>);
    std::println("std::generator<int>              lifetime_aware={}",
                 threadsafe::is_lifetime_aware_v<std::generator<int>>);
    std::println("std::span<int>                   lifetime_aware={}",
                 threadsafe::is_lifetime_aware_v<std::span<int>>);
    std::println("std::string_view                 sendable={}",
                 threadsafe::is_sendable_v<std::string_view>);
    std::println("std::coroutine_handle<>          sendable={}",
                 threadsafe::is_sendable_v<std::coroutine_handle<>>);
    std::println("plain_aggregate                  sendable={}",
                 threadsafe::is_sendable_v<plain_aggregate>);
}
