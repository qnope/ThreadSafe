#include <threadsafe/threadsafe.h>
#include <functional>
#include <print>

// 1. hijacking constructor template inherited into a derived class
struct HijackBase {
    int x = 0;
    HijackBase() = default;
    template <class U> HijackBase(U&& other) : x(other.x) {}
};
struct InheritsHijack : HijackBase { using HijackBase::HijackBase; };

// 2. same template, but the base is otherwise perfectly sendable and the
//    template lives in a *member's* type instead of the class itself
struct HijackMemberHost { HijackBase held; };

// 3. copy constructor taking a non-const lvalue reference
struct CopyFromNonConst {
    int x = 0;
    CopyFromNonConst() = default;
    CopyFromNonConst(CopyFromNonConst& other) : x(other.x) {}
};

// 4. private constructor template
class PrivateHijack {
    int x = 0;
    template <class U> PrivateHijack(U&&) {}
public:
    PrivateHijack() = default;
};

// 5. a constructor template that can never be selected for a copy
struct ArityTwoTemplate {
    int x = 0;
    ArityTwoTemplate() = default;
    template <class It> ArityTwoTemplate(It, It) {}
};

// 6. deducing-this call operator, no state
struct DeducingThis { void operator()(this DeducingThis) {} };

// 7. std::bind / bind_front / mem_fn / not_fn
void free_fn(int) {}
struct Obj { int field; void method() {} };
using BindResult      = decltype(std::bind(free_fn, 1));
using BindFrontResult = decltype(std::bind_front(free_fn, 1));
using MemFnResult     = decltype(std::mem_fn(&Obj::method));
using NotFnResult     = decltype(std::not_fn([] { return true; }));

#define ROW(...) std::println("{:<24} sendable={}", #__VA_ARGS__, threadsafe::is_sendable_v<__VA_ARGS__>)

int main() {
    ROW(HijackBase);
    ROW(InheritsHijack);
    ROW(HijackMemberHost);
    ROW(CopyFromNonConst);
    ROW(PrivateHijack);
    ROW(ArityTwoTemplate);
    ROW(DeducingThis);
    ROW(BindResult);
    ROW(BindFrontResult);
    ROW(MemFnResult);
    ROW(NotFnResult);
    std::println("InheritsHijack hijacks its own copy? trivially_constructible<InheritsHijack, InheritsHijack&> = {}",
                 std::is_trivially_constructible_v<InheritsHijack, InheritsHijack&>);
}
