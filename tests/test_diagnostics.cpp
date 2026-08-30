#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

// A trait answers with the reason it says no. This file checks both halves of
// that contract: an accepted type carries no reason, and a rejected one carries
// one — the message the user is meant to read.

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

// An accepted type has nothing to explain.
static_assert(is_sendable_v<Plain>.error_message == nullptr);

// A rejected one names why, so the message can be read back — by a
// static_assert, or by the launcher's explaining overload.
struct Borrowing {
    int *borrowed;
};

static_assert(carries_a_reason(is_sendable_v<Borrowing>));
static_assert(carries_a_reason(is_synchronizable_v<const Borrowing>));
static_assert(carries_a_reason(is_lifetime_aware_v<Borrowing>));

// Nesting answers no just the same: the walk reaches the borrow through as many
// hops as it takes.
struct BorrowingMiddle {
    Borrowing inner;
};

struct BorrowingOuter {
    BorrowingMiddle middle;
};

static_assert(carries_a_reason(is_sendable_v<BorrowingOuter>));
static_assert(carries_a_reason(is_synchronizable_v<const BorrowingOuter>));
static_assert(carries_a_reason(is_lifetime_aware_v<BorrowingOuter>));

// Three types the traits would each accept on their own: an int owns itself,
// races on nothing, and is safe to read through const.
struct Refused {
    int value;
};

struct RefusedSendable : Refused {};
struct RefusedSynchronizable : Refused {};
struct RefusedLifetimeAware : Refused {};

}

// A claim is final in both directions. Saying no is the half the trait cannot
// reach on its own — the walk has no way to know that a type it can read is
// nevertheless unfit — so it is the half worth stating, reason included.
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

// A refusal propagates like any other no: a member that was claimed unfit
// makes its holder unfit.
struct HoldsRefused {
    RefusedSendable member;
};

static_assert(!is_sendable_v<HoldsRefused>);

// Nobody claimed Refused, and that silence is not an answer: it must not read
// as a reason, and it must leave the trait to its own definition.
static_assert(!threadsafe::is_unsafe_sendable_v<Refused>.answered);
static_assert(!threadsafe::is_unsafe_lifetime_aware_v<Refused>.answered);
static_assert(threadsafe::is_unsafe_sendable_v<RefusedSendable>.answered);

}
