#include <threadsafe/threadsafe.h>

#include <any>
#include <coroutine>
#include <expected>
#include <filesystem>
#include <generator>
#include <optional>
#include <print>
#include <ranges>
#include <source_location>
#include <string_view>
#include <system_error>
#include <typeindex>
#include <vector>

union NamedUnion { int as_int; const char *as_pointer; };
struct HoldsNamedUnion { NamedUnion storage; };
struct HoldsAnonymousUnion { union { int as_int; const char *as_pointer; }; };
struct HoldsFunctionReference { void (&callback)(); };

template <class T>
void report(const char *label) {
    std::println("{:52} lifetime_aware={:5} sendable={:5}", label,
                 threadsafe::is_lifetime_aware_v<T>,
                 threadsafe::is_sendable_v<T>);
}

int main() {
    report<std::ranges::iota_view<int, int>>("std::ranges::iota_view<int,int>");
    report<std::ranges::empty_view<int>>("std::ranges::empty_view<int>");
    report<std::ranges::single_view<int>>("std::ranges::single_view<int>");
    report<std::ranges::repeat_view<int>>("std::ranges::repeat_view<int>");
    report<std::generator<int>>("std::generator<int>");
    report<std::coroutine_handle<>>("std::coroutine_handle<>");
    report<std::any>("std::any");
    report<std::expected<int, std::string_view>>("std::expected<int, std::string_view>");
    report<std::expected<int, int>>("std::expected<int,int>");
    report<std::filesystem::directory_iterator>("std::filesystem::directory_iterator");
    report<std::filesystem::path>("std::filesystem::path");
    report<std::source_location>("std::source_location");
    report<std::error_code>("std::error_code");
    report<std::type_index>("std::type_index");
    report<NamedUnion>("NamedUnion { int; const char*; }");
    report<HoldsNamedUnion>("HoldsNamedUnion");
    report<HoldsAnonymousUnion>("HoldsAnonymousUnion");
    report<HoldsFunctionReference>("struct { void (&callback)(); }");
    return 0;
}
