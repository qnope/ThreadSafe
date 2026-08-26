#include <threadsafe/threadsafe.h>
#include <list>
#include <ranges>
#include <optional>
#include <vector>

using threadsafe::is_lifetime_aware_v;
#define YES(NAME, ...) static_assert(is_lifetime_aware_v<__VA_ARGS__>, "GOT-FALSE: " NAME);
#define NO(NAME, ...)  static_assert(!is_lifetime_aware_v<__VA_ARGS__>, "GOT-TRUE: " NAME);

inline bool positive(int x) { return x > 0; }
using ListOwning = decltype(std::views::all(std::list<int>{}) | std::views::filter(&positive));

struct AnonUnionMember { union { int a; int *borrowed; }; };
struct NamedUnionMember { union U { int a; int *borrowed; } u; };
struct OptionalLike {
    union Storage { char none; int *borrowed; Storage() : none() {} ~Storage() {} } storage;
    bool engaged = false;
};
struct AnonUnionOnlyOwning { union { int a; double b; }; };

NO("filter over owning list view (cached list iterator)", ListOwning)
NO("anonymous union member hiding a pointer", AnonUnionMember)
NO("named union member hiding a pointer", NamedUnionMember)
NO("optional-like union hiding a pointer", OptionalLike)
YES("anonymous union of owning members", AnonUnionOnlyOwning)
NO("std::optional<int*>", std::optional<int*>)
int main(){}
