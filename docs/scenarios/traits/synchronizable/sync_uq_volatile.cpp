#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstdio>
#include <type_traits>

struct Vouched {};
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Vouched);

template <class T> consteval const char* a() { return threadsafe::is_synchronizable_v<T> ? "TRUE " : "false"; }
template <class T> consteval const char* s() { return threadsafe::is_sendable_v<T> ? "TRUE " : "false"; }
template <class T> consteval const char* i() { return threadsafe::is_synchronizable_type(^^T) ? "TRUE " : "false"; }
#define SHOW(...) std::printf("%-46s sync=%s  info=%s  send=%s\n", #__VA_ARGS__, a<__VA_ARGS__>(), i<__VA_ARGS__>(), s<__VA_ARGS__>())

int main() {
    std::printf("remove_cv_t<volatile int[4]> is int[4]: %d\n",
                std::is_same_v<std::remove_cv_t<volatile int[4]>, int[4]>);
    SHOW(std::atomic<int>);
    SHOW(volatile std::atomic<int>);
    SHOW(std::atomic<int>[4]);
    SHOW(volatile std::atomic<int>[4]);
    SHOW(const std::atomic<int>[4]);
    SHOW(const volatile std::atomic<int>[4]);
    SHOW(Vouched);
    SHOW(volatile Vouched);
    SHOW(volatile Vouched[4]);
    SHOW(void());
    SHOW(void() const);
    SHOW(int[]);
    SHOW(const int[]);
    SHOW(volatile int[]);
}
