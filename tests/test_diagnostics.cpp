#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

namespace {

struct Plain {
    int value;
    double ratio;
};

struct DerivedFromPlain : Plain {
    std::string name;
};

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(is_sendable_v<int>);
static_assert(is_sendable_v<Plain>);
static_assert(is_sendable_v<const Plain>);
static_assert(is_sendable_v<DerivedFromPlain>);
static_assert(is_sendable_v<std::atomic<int>>);
static_assert(is_sendable_v<int[3]>);

static_assert(is_synchronizable_v<std::atomic<int>>);
static_assert(is_synchronizable_v<const Plain>);
static_assert(is_synchronizable_v<const DerivedFromPlain>);
static_assert(is_synchronizable_v<std::atomic<int> *const>);
static_assert(is_synchronizable_v<const Plain[2]>);

static_assert(is_lifetime_aware_v<int>);
static_assert(is_lifetime_aware_v<Plain>);
static_assert(is_lifetime_aware_v<std::shared_ptr<int>>);
static_assert(is_lifetime_aware_v<DerivedFromPlain>);
static_assert(is_lifetime_aware_v<void (*)()>);
static_assert(is_lifetime_aware_v<Plain[2]>);

consteval bool carries_a_reason(threadsafe::TraitAnswer answer) {
    return !answer && answer.error_message[0] != '\0';
}

static_assert(is_sendable_v<Plain>.error_message == nullptr);

struct Borrowing {
    int *borrowed;
};

static_assert(carries_a_reason(is_sendable_v<Borrowing>));
static_assert(carries_a_reason(is_synchronizable_v<const Borrowing>));
static_assert(carries_a_reason(is_lifetime_aware_v<Borrowing>));

struct BorrowingMiddle {
    Borrowing inner;
};

struct BorrowingOuter {
    BorrowingMiddle middle;
};

static_assert(carries_a_reason(is_sendable_v<BorrowingOuter>));
static_assert(carries_a_reason(is_synchronizable_v<const BorrowingOuter>));
static_assert(carries_a_reason(is_lifetime_aware_v<BorrowingOuter>));

struct Refused {
    int value;
};

struct RefusedSendable : Refused {};
struct RefusedSynchronizable : Refused {};
struct RefusedLifetimeAware : Refused {};

}

template <>
struct threadsafe::is_unsafe_sendable<RefusedSendable> {
    static constexpr threadsafe::TraitAnswer value = "refused by this test";
};

template <>
struct threadsafe::is_unsafe_synchronizable<const RefusedSynchronizable> {
    static constexpr threadsafe::TraitAnswer value = "refused by this test";
};

template <>
struct threadsafe::is_unsafe_lifetime_aware<RefusedLifetimeAware> {
    static constexpr threadsafe::TraitAnswer value = "refused by this test";
};

namespace {

consteval bool reason_is(threadsafe::TraitAnswer answer, std::string_view text) {
    return !answer && std::string_view(answer.error_message) == text;
}

static_assert(is_sendable_v<Refused>);
static_assert(is_synchronizable_v<const Refused>);
static_assert(is_lifetime_aware_v<Refused>);

static_assert(reason_is(is_sendable_v<RefusedSendable>, "refused by this test"));
static_assert(reason_is(is_synchronizable_v<const RefusedSynchronizable>,
                        "refused by this test"));
static_assert(reason_is(is_lifetime_aware_v<RefusedLifetimeAware>,
                        "refused by this test"));

struct HoldsRefused {
    RefusedSendable member;
};

static_assert(!is_sendable_v<HoldsRefused>);

static_assert(!threadsafe::is_unsafe_sendable_v<Refused>.answered);
static_assert(!threadsafe::is_unsafe_lifetime_aware_v<Refused>.answered);
static_assert(threadsafe::is_unsafe_sendable_v<RefusedSendable>.answered);

}
