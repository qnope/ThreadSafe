#include <threadsafe/threadsafe.h>

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

struct Plain { int a; };
struct UserCopy { UserCopy(const UserCopy&); };
struct SyncType {};
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);
struct OptIn {};
template <> struct threadsafe::is_sendable<OptIn> : std::true_type {};

template <class T>
consteval bool agrees() {
    return threadsafe::is_sendable_v<T> == threadsafe::is_sendable_v<const T>
        && threadsafe::is_sendable_v<T> == threadsafe::is_sendable_v<volatile T>
        && threadsafe::is_sendable_v<T> == threadsafe::is_sendable_v<const volatile T>;
}

#define ROW(...) std::println("{:<40} T={:<5} const T={:<5} volatile T={:<5} agrees={}", \
    #__VA_ARGS__, threadsafe::is_sendable_v<__VA_ARGS__>, \
    threadsafe::is_sendable_v<const __VA_ARGS__>, \
    threadsafe::is_sendable_v<volatile __VA_ARGS__>, agrees<__VA_ARGS__>())

int main() {
    ROW(int);
    ROW(int*);
    ROW(std::atomic<int>*);
    ROW(int[4]);
    ROW(int*[4]);
    ROW(Plain);
    ROW(UserCopy);
    ROW(SyncType);
    ROW(OptIn);
    ROW(std::vector<int>);
    ROW(std::vector<int*>);
    ROW(std::string);
    ROW(std::array<int, 4>);
    ROW(std::optional<int>);
    ROW(std::tuple<int, double>);
    ROW(std::variant<int, double>);
    ROW(std::pair<int, int*>);
    ROW(std::unique_ptr<int>);
    ROW(std::shared_ptr<int>);
    ROW(void (*)());
    ROW(std::chrono::milliseconds);
    ROW(std::nullptr_t);
}
