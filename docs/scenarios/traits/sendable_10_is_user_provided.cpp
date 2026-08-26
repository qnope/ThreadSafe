#include <meta>
#include <cstdio>

namespace {
struct A { int v; ~A(); A(const A&); };
struct B { int v; ~B() = default; B(const B&) = default; };
struct C { int v; ~C(); };
}

consteval bool probe(std::meta::info type, bool want_defaulted, bool want_user_provided) {
    for (std::meta::info m : std::meta::members_of(type, std::meta::access_context::unchecked()))
        if (std::meta::is_destructor(m))
            return std::meta::is_defaulted(m) == want_defaulted
                && std::meta::is_user_provided(m) == want_user_provided;
    return false;
}

// before the out-of-line definitions
static_assert(probe(^^A, /*defaulted*/false, /*user_provided*/true));
static_assert(probe(^^B, true, false));
static_assert(probe(^^C, false, true));

namespace {
A::~A() = default;
A::A(const A&) = default;
}

// after: is_defaulted flipped, is_user_provided did NOT
static_assert(probe(^^A, /*defaulted*/true,  /*user_provided*/true),
              "is_user_provided is stable across the out-of-line = default");
