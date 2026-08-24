#include <threadsafe/threadsafe.h>

#include <atomic>
#include <barrier>
#include <condition_variable>
#include <latch>
#include <mutex>
#include <semaphore>
#include <shared_mutex>
#include <stop_token>
#include <string>
#include <vector>

#include <cstdio>

using threadsafe::is_synchronizable_v;

struct Incomplete;
union PlainUnion { int a; double b; };
struct Bitfields { int a : 3; int : 5; unsigned b : 2; };
struct VBase { virtual ~VBase() = default; };
struct D1 : virtual VBase {};
struct D2 : virtual VBase {};
struct Diamond : D1, D2 {};
struct Empty {};
struct EBO : Empty { int i; };
struct HasAnonUnion { union { int a; float b; }; int c; };
struct PtrToMemberHolder { int Empty::* pm; };

template <class T>
consteval const char* answer() { return is_synchronizable_v<T> ? "TRUE " : "false"; }

#define SHOW(...) std::printf("%-56s %s\n", #__VA_ARGS__, answer<__VA_ARGS__>())

int main() {
    SHOW(int);
    SHOW(const int);
    SHOW(volatile int);
    SHOW(const volatile int);
    SHOW(int&);
    SHOW(int&&);
    SHOW(int*);
    SHOW(void);
    SHOW(const void);
    SHOW(void());
    SHOW(void() const);
    SHOW(void() &&);
    SHOW(void(&)());
    SHOW(void(*)());
    SHOW(int Empty::*);
    SHOW(void (Empty::*)());
    SHOW(int[4]);
    SHOW(int[]);
    SHOW(const int[4]);
    SHOW(const int[]);
    SHOW(volatile int[4]);
    SHOW(const volatile int[4]);
    SHOW(int(&)[4]);
    SHOW(std::atomic<int>);
    SHOW(std::atomic<int>[4]);
    SHOW(std::atomic<int>[4][4]);
    SHOW(const std::atomic<int>[4]);
    SHOW(std::atomic<int*>);
    SHOW(std::atomic<void(*)()>);
    SHOW(std::atomic<std::shared_ptr<int>>);
    SHOW(std::atomic<std::shared_ptr<std::atomic<int>>>);
    SHOW(std::atomic<std::vector<int>>);
    SHOW(std::atomic<std::string>);
    SHOW(std::atomic_flag);
    SHOW(std::atomic_ref<int>);
    SHOW(std::mutex);
    SHOW(std::shared_mutex);
    SHOW(std::latch);
    SHOW(std::barrier<>);
    SHOW(std::counting_semaphore<>);
    SHOW(std::condition_variable);
    SHOW(std::stop_token);
    SHOW(std::stop_source);
    SHOW(PlainUnion);
    SHOW(Bitfields);
    SHOW(Diamond);
    SHOW(EBO);
    SHOW(HasAnonUnion);
    SHOW(PtrToMemberHolder);
    SHOW(Incomplete);
    SHOW(threadsafe::synchronized_value<int>);
    SHOW(threadsafe::copy_on_write<int>);
}
