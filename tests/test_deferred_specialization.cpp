#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(is_lifetime_aware_v<std::vector<int>>);
static_assert(is_lifetime_aware_v<std::string>);
static_assert(is_sendable_v<std::vector<int>>);
static_assert(is_sendable_v<std::string>);

static_assert(is_sendable_v<std::unique_ptr<int>>);

static_assert(is_synchronizable_v<std::atomic<int>>);
static_assert(is_sendable_v<std::atomic<int>&>,
              "sending a reference to an atomic shares a synchronizable object");
static_assert(is_sendable_v<void (*)()>,
              "function pointers rely on function types being synchronizable");

namespace {
struct Opaque {
    int* borrowed;
};
struct Holder {
    Opaque o;
};
}

template <>
struct threadsafe::is_unsafe_synchronizable<Opaque> {
    static consteval threadsafe::TraitAnswer diagnose() {
        return {};
    }
};

static_assert(!is_sendable_v<int*>);
static_assert(is_sendable_v<Holder>,
              "a specialization declared in the user's TU must reach the "
              "recursion over members");

static_assert(threadsafe::is_sendable_type(^^Holder),
              "the info-level face of the trait answers like is_sendable_v<T>");
static_assert(threadsafe::is_synchronizable_type(^^Opaque));
static_assert(threadsafe::is_lifetime_aware_type(^^std::string));
static_assert(threadsafe::is_synchronizable_type(^^const std::vector<int>),
              "the const rule reads specializations back through substitute too");
