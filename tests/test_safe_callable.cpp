#include <threadsafe/safe_callable.h>

#include <functional>

namespace {
struct EmptyCallable {
    void operator()() const {}
};
struct EmptyTag {};
struct StatefulCallable {
    int state = 0;
    void operator()() { ++state; }
};
struct SyncCallable {
    std::atomic<int> counter{0};
    void operator()() const {}
};
}  // namespace

template <>
constexpr bool threadsafe::is_synchronizable<SyncCallable> = true;

using threadsafe::is_safe_callable;

// --- functions: code is immutable, invocable from any thread ---
static_assert(is_safe_callable<void (*)()>,
              "is_safe_callable — function pointers are safe callables");
static_assert(is_safe_callable<int (*)(int)>,
              "is_safe_callable — function pointers are safe callables");
static_assert(is_safe_callable<void()>,
              "is_safe_callable — function types are synchronizable, hence"
              " safe callables");

// --- empty callables: no per-object state to race on ---
static_assert(is_safe_callable<decltype([] {})>,
              "is_safe_callable — a captureless lambda has no state");
static_assert(is_safe_callable<decltype([](auto) {})>,
              "is_safe_callable — a captureless generic lambda has no state");
static_assert(is_safe_callable<decltype([]() mutable {})>,
              "is_safe_callable — mutable changes nothing when there is no"
              " state to mutate");
static_assert(is_safe_callable<EmptyCallable>,
              "is_safe_callable — an empty class with operator() has no state");
static_assert(is_safe_callable<EmptyTag>,
              "is_safe_callable — the trait states safety, not invocability:"
              " any empty class qualifies");

// --- stateful callables are not safe to share ---
static_assert(!is_safe_callable<decltype([x = 42] {})>,
              "is_safe_callable — a capturing lambda carries per-object state");
static_assert(!is_safe_callable<StatefulCallable>,
              "is_safe_callable — a non-empty callable carries per-object"
              " state");
static_assert(!is_safe_callable<std::function<void()>>,
              "is_safe_callable — std::function owns unsynchronized state");

// --- synchronizable callables opt in ---
static_assert(is_safe_callable<SyncCallable>,
              "is_safe_callable — a synchronizable callable may already be"
              " shared across threads");

// --- non-callables and out-of-scope pointers ---
static_assert(!is_safe_callable<int>,
              "is_safe_callable — default is false");
static_assert(!is_safe_callable<void (EmptyCallable::*)()>,
              "is_safe_callable — member function pointers are out of scope"
              " for now");
