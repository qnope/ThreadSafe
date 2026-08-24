#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstdio>
namespace threadsafe {
template <class T> struct is_synchronizable<volatile T> : is_synchronizable<T> {};
template <class T> struct is_synchronizable<const volatile T> : is_synchronizable<const T> {};
template <class T, std::size_t N> struct is_synchronizable<volatile T[N]> : is_synchronizable<T> {};
template <class T> struct is_synchronizable<volatile T[]> : is_synchronizable<T> {};
template <class T, std::size_t N> struct is_synchronizable<const volatile T[N]> : is_synchronizable<const T> {};
template <class T> struct is_synchronizable<const volatile T[]> : is_synchronizable<const T> {};
}
template <class T> consteval const char* a() { return threadsafe::is_synchronizable_v<T> ? "TRUE " : "false"; }
#define SHOW(...) std::printf("%-42s %s\n", #__VA_ARGS__, a<__VA_ARGS__>())
int main() {
    SHOW(volatile std::atomic<int>);      SHOW(volatile std::atomic<int>[4]);
    SHOW(const volatile std::atomic<int>);SHOW(const volatile std::atomic<int>[4]);
    SHOW(volatile int); SHOW(const volatile int); SHOW(volatile int[4]); SHOW(const volatile int[4]);
    SHOW(const int); SHOW(const int[4]); SHOW(int[4]); SHOW(std::atomic<int>[4]);
    SHOW(volatile int[]); SHOW(const volatile int[]);
}
