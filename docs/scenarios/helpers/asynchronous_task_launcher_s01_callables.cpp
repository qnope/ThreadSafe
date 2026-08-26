#include <threadsafe/threadsafe.h>

#include <functional>
#include <memory>
#include <string>
#include <print>

namespace {
template <class F, class... Args>
constexpr bool can_launch_task = threadsafe::launchable_task<F, Args...>;
template <class F, class... Args>
constexpr bool can_launch_scoped = threadsafe::launchable_scoped_task<F, Args...>;

struct Widget { int value = 0; };

int global_local = 0;

auto make_ref_capture(int& x)      { return [&x] { x++; }; }
auto make_ptr_by_value(int* p)     { return [p] { *p = 1; }; }
auto make_string_by_value()        { std::string s = "hello"; return [s] { (void)s.size(); }; }
auto make_shared_capture()         { auto sp = std::make_shared<int>(1); return [sp] { (void)*sp; }; }
auto make_generic()                { return [](auto x) { (void)x; }; }
auto make_mutable()                { int n = 0; return [n]() mutable { n++; }; }
auto make_init_move_unique()       { auto up = std::make_unique<int>(3); return [u = std::move(up)] { (void)*u; }; }

using RefCapture      = decltype(make_ref_capture(global_local));
using PtrByValue      = decltype(make_ptr_by_value(&global_local));
using StringByValue   = decltype(make_string_by_value());
using SharedCapture   = decltype(make_shared_capture());
using Generic         = decltype(make_generic());
using MutableLambda   = decltype(make_mutable());
using InitMoveUnique  = decltype(make_init_move_unique());

struct Holder {
    int member = 0;
    auto capture_this() { return [this] { member++; }; }
};
using ThisCapture = decltype(std::declval<Holder&>().capture_this());

void free_function(int) {}

struct FunctorRawPointer {
    int* borrowed;
    void operator()() const { (void)borrowed; }
};

struct FunctorStatic {
    static inline int shared_counter = 0;
    void operator()() const { shared_counter++; }
};

struct StatelessUserCopy {
    StatelessUserCopy() = default;
    StatelessUserCopy(const StatelessUserCopy&) {}
    void operator()() const {}
};
}

using threadsafe::is_sendable_v;
using threadsafe::is_lifetime_aware_v;

// ---- reference capture: must be rejected
static_assert(!is_sendable_v<RefCapture>);
static_assert(!can_launch_task<RefCapture>);
static_assert(!can_launch_scoped<RefCapture>);

// ---- raw pointer captured BY VALUE: the closure has no reflectable members
static_assert(!is_sendable_v<PtrByValue>, "POLARITY: pointer-by-value capture");
static_assert(!can_launch_task<PtrByValue>);
static_assert(!can_launch_scoped<PtrByValue>);

// ---- std::string captured BY VALUE: perfectly safe to send
static_assert(!is_sendable_v<StringByValue>, "POLARITY: string-by-value capture");
static_assert(!can_launch_task<StringByValue>);
static_assert(!can_launch_scoped<StringByValue>);

// ---- [this]
static_assert(!can_launch_task<ThisCapture>);

// ---- shared_ptr capture
static_assert(!can_launch_task<SharedCapture>);

// ---- generic lambda (captureless -> empty)
static_assert(can_launch_task<Generic>, "POLARITY: generic captureless lambda");
static_assert(is_sendable_v<Generic> && is_lifetime_aware_v<Generic>);

// ---- mutable lambda capturing an int
static_assert(!can_launch_task<MutableLambda>);

// ---- init-capture moving a unique_ptr
static_assert(!can_launch_task<InitMoveUnique>);

// ---- std::bind result
using BindResult = decltype(std::bind(&free_function, 42));
static_assert(!can_launch_task<BindResult>, "POLARITY: std::bind");

// ---- std::function (type erased!)
static_assert(!can_launch_task<std::function<void()>>, "POLARITY: std::function");
static_assert(!can_launch_scoped<std::function<void()>>);
static_assert(!is_sendable_v<std::function<void()>>);

// ---- std::move_only_function
static_assert(!can_launch_task<std::move_only_function<void()>>, "POLARITY: move_only_function");

// ---- function pointer
static_assert(can_launch_task<void(*)(int), int>, "POLARITY: function pointer");

// ---- pointer to member function + object
static_assert(can_launch_task<decltype(&Holder::capture_this), Holder>,
              "POLARITY: pmf");
static_assert(is_sendable_v<int Holder::*>, "POLARITY: pointer-to-data-member sendable?");
static_assert(is_lifetime_aware_v<int Holder::*>, "POLARITY: pointer-to-data-member lifetime?");

// ---- functor with a raw pointer member
static_assert(!can_launch_task<FunctorRawPointer>);

// ---- functor touching a static member: NO member state at all
static_assert(can_launch_task<FunctorStatic>, "POLARITY: static-member functor accepted");

// ---- stateless functor with a user-written copy constructor
static_assert(!can_launch_task<StatelessUserCopy>);

int main() { std::println("ok"); }
