#include <threadsafe/threadsafe.h>
#include <atomic>
#include <print>
#include <string>

struct MutableStatic     { static inline long long total = 0; int weight = 1; };
struct ConstexprStatic   { static constexpr int limit = 8; int weight = 1; };
struct ConstStringStatic { static inline const std::string banner = "hi"; int weight = 1; };
struct AtomicStatic      { static inline std::atomic<long long> total{0}; int weight = 1; };
struct ThreadLocalStatic { static thread_local long long total; int weight = 1; };
struct ConstPointerStatic{ static inline int* const cursor = nullptr; int weight = 1; };
thread_local long long ThreadLocalStatic::total = 0;

#define ROW(...) std::println("{:<22} sendable={}", #__VA_ARGS__, threadsafe::is_sendable_v<__VA_ARGS__>)
int main() {
    ROW(MutableStatic);
    ROW(ConstexprStatic);
    ROW(ConstStringStatic);
    ROW(AtomicStatic);
    ROW(ThreadLocalStatic);
    ROW(ConstPointerStatic);
}
