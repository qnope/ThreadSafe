#include <concepts>
#include <type_traits>
#include <string>
#include <utility>

// Ground truth: does copy-initialising from a NON-CONST lvalue run user code?
// A struct whose template ctor sets a flag tells us which candidate won.

struct GreedyForward {
    bool hijacked_ = false;
    GreedyForward() = default;
    template <class A> constexpr GreedyForward(A &&) : hijacked_(true) {}
};
struct GreedyLvalue {
    bool hijacked_ = false;
    GreedyLvalue() = default;
    template <class A> constexpr GreedyLvalue(A &) : hijacked_(true) {}
};
struct HarmlessConstRef {
    bool hijacked_ = false;
    HarmlessConstRef() = default;
    template <class A> constexpr HarmlessConstRef(const A &) : hijacked_(true) {}
};
struct ByValueTemplate {
    bool hijacked_ = false;
    ByValueTemplate() = default;
    template <class A> constexpr ByValueTemplate(A) : hijacked_(true) {}
};
struct GuardedForward {
    bool hijacked_ = false;
    GuardedForward() = default;
    template <class A> requires (!std::same_as<std::remove_cvref_t<A>, GuardedForward>)
    constexpr GuardedForward(A &&) : hijacked_(true) {}
};
struct VariadicForwarding {
    bool hijacked_ = false;
    VariadicForwarding() = default;
    template <class... A> constexpr VariadicForwarding(A &&...) : hijacked_(true) {}
};
struct VariadicByValue {
    bool hijacked_ = false;
    VariadicByValue() = default;
    template <class... A> constexpr VariadicByValue(A...) : hijacked_(true) {}
};

template <class T>
constexpr bool copy_from_lvalue_runs_user_code() {
    T source{};
    T destination = source;          // source is a NON-CONST lvalue
    return destination.hijacked_;
}

static_assert( copy_from_lvalue_runs_user_code<GreedyForward>(),      "GreedyForward does NOT hijack");
static_assert( copy_from_lvalue_runs_user_code<GreedyLvalue>(),       "GreedyLvalue does NOT hijack");
static_assert(!copy_from_lvalue_runs_user_code<HarmlessConstRef>(),   "HarmlessConstRef DOES hijack");
static_assert(!copy_from_lvalue_runs_user_code<ByValueTemplate>(),    "ByValueTemplate DOES hijack");
static_assert(!copy_from_lvalue_runs_user_code<GuardedForward>(),     "GuardedForward DOES hijack");
static_assert( copy_from_lvalue_runs_user_code<VariadicForwarding>(), "VariadicForwarding does NOT hijack");
static_assert(!copy_from_lvalue_runs_user_code<VariadicByValue>(),    "VariadicByValue DOES hijack");
