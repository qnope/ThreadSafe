#include <threadsafe/threadsafe.h>
#include <memory>
#include <span>
#include <vector>
#include <string>

using threadsafe::is_lifetime_aware_v;
#define YES(NAME, ...) static_assert(is_lifetime_aware_v<__VA_ARGS__>, "EXPECTED-TRUE-GOT-FALSE: " NAME);
#define NO(NAME, ...)  static_assert(!is_lifetime_aware_v<__VA_ARGS__>, "EXPECTED-FALSE-GOT-TRUE: " NAME);

struct Pool { void release(int*); };
struct BorrowingDeleter { Pool *pool; void operator()(int *p) const { pool->release(p); } };
struct OwningDeleter { void operator()(int *p) const { delete p; } };

union PlainUnion { int a; double b; };
union BorrowUnion { int a; std::span<int> b; };
union PtrUnion { int a; int *p; };

auto captureless = [](int x) { return x; };
auto captures_ref = [&captureless](int x) { return captureless(x); };
int global_int = 0;
auto captures_ptr = [p = &global_int]() { return *p; };
auto captures_value = [v = std::string("x")]() { return v.size(); };

struct SelfShared { std::shared_ptr<SelfShared> next; };
struct SelfUnique { std::unique_ptr<SelfUnique> next; };
struct SelfVector { std::vector<SelfVector> kids; };

NO("unique_ptr with borrowing deleter", std::unique_ptr<int, BorrowingDeleter>)
YES("unique_ptr with owning deleter", std::unique_ptr<int, OwningDeleter>)
YES("shared_ptr<int> (deleter type-erased)", std::shared_ptr<int>)

YES("plain union", PlainUnion)
NO("union with span", BorrowUnion)
NO("union with pointer", PtrUnion)

YES("captureless lambda", decltype(captureless))
NO("lambda capturing by reference", decltype(captures_ref))
NO("lambda capturing a pointer", decltype(captures_ptr))
YES("lambda capturing by value", decltype(captures_value))

YES("self-referential via shared_ptr", SelfShared)
YES("self-referential via unique_ptr", SelfUnique)
YES("self-referential via vector", SelfVector)
int main(){}
