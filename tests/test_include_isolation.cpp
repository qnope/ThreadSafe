#include <threadsafe/details/lifetime_aware.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

using threadsafe::is_lifetime_aware;
using threadsafe::is_sendable;
using threadsafe::is_synchronizable;

static_assert(is_lifetime_aware<std::vector<int>>);
static_assert(is_lifetime_aware<std::string>);
static_assert(is_sendable<std::vector<int>>);
static_assert(is_sendable<std::string>);

static_assert(is_sendable<std::unique_ptr<int>>);

static_assert(is_synchronizable<std::atomic<int>>);
static_assert(is_sendable<std::atomic<int>&>,
              "sending a reference to an atomic shares a synchronizable object");
static_assert(is_sendable<void (*)()>,
              "function pointers rely on function types being synchronizable");

// The traits read each other back through the variable template, from a
// std::meta::info, via std::meta::substitute. That resolves at evaluation time,
// so a specialization written after the header — here, in this translation unit
// — is still the answer when the recursion reaches the member. Were it to
// resolve where the substitute call is written instead, Holder would be judged
// on Opaque's raw pointer and come out non-sendable.
namespace {
struct Opaque {
    int* borrowed;
};
struct Holder {
    Opaque o;
};
}

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Opaque);

static_assert(!is_sendable<int*>);
static_assert(is_sendable<Holder>,
              "a specialization declared in the user's TU must reach the "
              "recursion over members");

static_assert(threadsafe::is_sendable_type(^^Holder),
              "the info-level face of the trait answers like is_sendable<T>");
static_assert(threadsafe::is_synchronizable_type(^^Opaque));
static_assert(threadsafe::is_lifetime_aware_type(^^std::string));
static_assert(threadsafe::is_synchronizable_type(^^const std::vector<int>),
              "the const rule reads specializations back through substitute too");
