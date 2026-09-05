#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

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

struct Borrowing {
    int *borrowed;
};

static_assert(!is_sendable_v<Borrowing>);
static_assert(!is_synchronizable_v<const Borrowing>);
static_assert(!is_lifetime_aware_v<Borrowing>);

struct BorrowingMiddle {
    Borrowing inner;
};

struct BorrowingOuter {
    BorrowingMiddle middle;
};

static_assert(!is_sendable_v<BorrowingOuter>);
static_assert(!is_synchronizable_v<const BorrowingOuter>);
static_assert(!is_lifetime_aware_v<BorrowingOuter>);

struct DerivedFromBorrowing : BorrowingOuter {};

static_assert(!is_sendable_v<DerivedFromBorrowing>);

static_assert(!is_sendable_v<int *>);
static_assert(!is_sendable_v<Borrowing[4]>);
static_assert(!is_sendable_v<std::vector<Borrowing>>);
static_assert(!is_sendable_v<std::shared_ptr<Borrowing>>);
static_assert(!is_lifetime_aware_v<std::unique_ptr<Borrowing>>);

struct Refused {
    int value;
};

struct RefusedSendable : Refused {};

}

template <>
struct threadsafe::is_unsafe_sendable<RefusedSendable> : std::true_type {};

namespace {

static_assert(is_sendable_v<Refused>);
static_assert(is_synchronizable_v<const Refused>);
static_assert(is_lifetime_aware_v<Refused>);

static_assert(!threadsafe::is_unsafe_sendable_v<Refused>);
static_assert(!threadsafe::is_unsafe_lifetime_aware_v<Refused>);
static_assert(threadsafe::is_unsafe_sendable_v<RefusedSendable>);

struct HoldsVouched {
    RefusedSendable member;
};

static_assert(is_sendable_v<HoldsVouched>);

}
