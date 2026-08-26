#include <threadsafe/threadsafe.h>
#include <concepts>
#include <type_traits>

namespace {
struct ForwardingCtor { int x = 0; ForwardingCtor() = default;
    template <class U> ForwardingCtor(U&& other) : x(other.x) {} };
struct ConstRefCtorTemplate { int x = 0; ConstRefCtorTemplate() = default;
    template <class U> ConstRefCtorTemplate(const U& other) : x(other.x) {} };
struct GuardedForwardingCtor { int x = 0; GuardedForwardingCtor() = default;
    template <class U> requires(!std::same_as<std::remove_cvref_t<U>, GuardedForwardingCtor>)
    GuardedForwardingCtor(U&& other) : x(other.x) {} };
struct ForwardingAssign { int x = 0;
    template <class U> ForwardingAssign& operator=(U&& o) { x = o.x; return *this; } };
struct GuardedForwardingAssign { int x = 0;
    template <class U> requires(!std::same_as<std::remove_cvref_t<U>, GuardedForwardingAssign>)
    GuardedForwardingAssign& operator=(U&& o) { x = o.x; return *this; } };
struct ByValueCtorTemplate { int x = 0; ByValueCtorTemplate() = default;
    template <class U> ByValueCtorTemplate(U other) : x(other.x) {} };
struct IteratorPairCtor { int x = 0; IteratorPairCtor() = default;
    template <class It> IteratorPairCtor(It, It) {} };
struct DefaultedSecondParam { int x = 0; DefaultedSecondParam() = default;
    template <class U> DefaultedSecondParam(U&& o, int tag = 0) : x(o.x + tag) {} };
struct NonTypeCtorTemplate { int x = 0; NonTypeCtorTemplate() = default;
    template <int N> NonTypeCtorTemplate(std::integral_constant<int, N>) {} };
}

// Ground truth from the language: the hijack is observable as "copy-constructing
// from a non-const lvalue is not the trivial implicit copy".
#define REALLY_HIJACKS(T) (!std::is_trivially_constructible_v<T, T&>)
static_assert(REALLY_HIJACKS(ForwardingCtor));
static_assert(!REALLY_HIJACKS(ConstRefCtorTemplate));
static_assert(!REALLY_HIJACKS(GuardedForwardingCtor));
static_assert(!REALLY_HIJACKS(ByValueCtorTemplate));
static_assert(!REALLY_HIJACKS(IteratorPairCtor));
static_assert(REALLY_HIJACKS(DefaultedSecondParam));
static_assert(!REALLY_HIJACKS(NonTypeCtorTemplate));
#define REALLY_HIJACKS_ASSIGN(T) (!std::is_trivially_assignable_v<T&, T&>)
static_assert(REALLY_HIJACKS_ASSIGN(ForwardingAssign));
static_assert(!REALLY_HIJACKS_ASSIGN(GuardedForwardingAssign));

// What the library answers.
using threadsafe::is_sendable_v;
static_assert(!is_sendable_v<ForwardingCtor>, "true hijack stays rejected");
static_assert(!is_sendable_v<ForwardingAssign>, "true hijack stays rejected");
static_assert(!is_sendable_v<DefaultedSecondParam>, "defaulted 2nd param still hijacks");
static_assert(is_sendable_v<ConstRefCtorTemplate>, "harmless const& template accepted");
static_assert(is_sendable_v<GuardedForwardingCtor>, "guarded template accepted");
static_assert(is_sendable_v<GuardedForwardingAssign>, "guarded assign accepted");
static_assert(is_sendable_v<ByValueCtorTemplate>, "by-value template accepted");
static_assert(is_sendable_v<IteratorPairCtor>, "iterator-pair ctor accepted");
static_assert(is_sendable_v<NonTypeCtorTemplate>, "non-type template accepted");
int main(){}
