#include <threadsafe/threadsafe.h>

#include <atomic>
#include <print>
#include <string>

struct Plain { int a; std::string b; };
struct SyncType {};
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);

// remove_cv on the pointee/referent asks the FULL trait where keeping the const
// would have asked the const one. The const rule is structural and often true;
// the full one is opt-in. So stripping can only ever make the answer harder --
// never easier. Every column below must read stripped <= kept.
#define ROW(...)                                                                \
    std::println("{:<26} sync<T>={:<5} sync<const T>={:<5} is_sendable<const T*>={}", \
                 #__VA_ARGS__, threadsafe::is_synchronizable_v<__VA_ARGS__>,    \
                 threadsafe::is_synchronizable_v<const __VA_ARGS__>,            \
                 threadsafe::is_sendable_v<const __VA_ARGS__*>)

int main() {
    ROW(int);
    ROW(Plain);
    ROW(std::string);
    ROW(SyncType);
    ROW(std::atomic<int>);
    std::println("is_sendable<const int (*)[4]> = {}  (kept-const would have said {})",
                 threadsafe::is_sendable_v<const int (*)[4]>,
                 threadsafe::is_synchronizable_v<const int[4]>);
}
