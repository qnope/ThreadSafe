#include <threadsafe/threadsafe.h>
#include <vector>
#include <list>
#include <map>
#include <deque>
#include <iterator>
#include <iostream>
#include <string>
#include <cstddef>

using threadsafe::is_lifetime_aware_v;
#define PROBE(NAME, ...) static_assert(is_lifetime_aware_v<__VA_ARGS__>, "FALSE: " NAME);

struct X { int m; void f(); virtual ~X() = default; };
struct HoldsFnPtr { void (*fn)(); };
struct HoldsPmf { void (X::*pmf)(); };
enum E { e0 };
enum class EC { a };

PROBE("vector<int>::iterator", std::vector<int>::iterator)
PROBE("vector<int>::const_iterator", std::vector<int>::const_iterator)
PROBE("list<int>::iterator", std::list<int>::iterator)
PROBE("map<int,int>::iterator", std::map<int,int>::iterator)
PROBE("deque<int>::iterator", std::deque<int>::iterator)
PROBE("raw pointer as iterator int*", int*)
PROBE("back_insert_iterator<vector<int>>", std::back_insert_iterator<std::vector<int>>)
PROBE("istream_iterator<int>", std::istream_iterator<int>)
PROBE("ostream_iterator<int>", std::ostream_iterator<int>)
PROBE("reverse_iterator<vector<int>::iterator>", std::reverse_iterator<std::vector<int>::iterator>)
PROBE("move_iterator<int*>", std::move_iterator<int*>)
PROBE("counted_iterator<int*>", std::counted_iterator<int*>)

PROBE("void(*)()", void(*)())
PROBE("void(*)() noexcept", void(*)() noexcept)
PROBE("int(*)(int,int)", int(*)(int,int))
PROBE("void(&)()", void(&)())
PROBE("function type void()", void())
PROBE("pointer to member function", void (X::*)())
PROBE("pointer to data member", int X::*)
PROBE("array of fn ptrs", void(*[4])())
PROBE("struct holding fn ptr", HoldsFnPtr)
PROBE("struct holding pmf", HoldsPmf)

PROBE("enum E", E)
PROBE("enum class EC", EC)
PROBE("nullptr_t", std::nullptr_t)
PROBE("double", double)
PROBE("bool", bool)
int main(){}
