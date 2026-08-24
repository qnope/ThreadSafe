#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstdio>

struct Incomplete;
struct SelfAtomic { std::atomic<SelfAtomic*> next; };
struct MutualA; struct MutualB;
struct MutualA { std::atomic<MutualB*> b; };
struct MutualB { std::atomic<MutualA*> a; };
struct Vouched { int* raw; };
namespace deep { struct Nested { int* raw; }; }

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Vouched);
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(deep::Nested);
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(std::pair<int, int>);   // comma survives __VA_ARGS__
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(int[4]);
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(const Incomplete);

template <class T> consteval const char* a() { return threadsafe::is_synchronizable_v<T> ? "TRUE " : "false"; }
#define SHOW(...) std::printf("%-52s %s\n", #__VA_ARGS__, a<__VA_ARGS__>())

int main() {
    SHOW(Vouched); SHOW(Vouched[4]); SHOW(const Vouched); SHOW(const Vouched[4]);
    SHOW(volatile Vouched); SHOW(const volatile Vouched); SHOW(Vouched[]);
    SHOW(std::atomic<Vouched*>);
    SHOW(deep::Nested);
    SHOW(std::pair<int, int>);
    SHOW(int[4]);                 // macro'd whole array type
    SHOW(int[3]);                 // neighbour untouched
    SHOW(const Incomplete);
    SHOW(SelfAtomic); SHOW(MutualA); SHOW(MutualB);
    SHOW(const SelfAtomic); SHOW(const MutualA);
    SHOW(threadsafe::synchronized_value<int>);
    SHOW(const threadsafe::synchronized_value<int>);
    SHOW(const std::stop_source);
    SHOW(std::atomic<Incomplete*>);
    SHOW(int[4][4]); SHOW(const int[4][4]);
}
