#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <string>

// The assert_* functions are the diagnostic face of the traits: they agree with
// the trait on a conforming type (they compile and return), and turn a "false"
// into a std::meta::exception naming the culprit. Only the agreeing half is
// testable here — the throwing half *is* a compile error by design.

namespace {

struct Plain {
    int value;
    double ratio;
};

struct DerivedFromPlain : Plain {
    std::string name;
};

consteval bool sendable_diagnostics_agree() {
    threadsafe::assert_sendable<int>();
    threadsafe::assert_sendable<Plain>();
    threadsafe::assert_sendable<const Plain>();
    threadsafe::assert_sendable<DerivedFromPlain>();
    threadsafe::assert_sendable<std::atomic<int>>();
    threadsafe::assert_sendable<int[3]>();
    return true;
}

consteval bool synchronizable_diagnostics_agree() {
    threadsafe::assert_synchronizable<std::atomic<int>>();
    threadsafe::assert_synchronizable<const Plain>();
    threadsafe::assert_synchronizable<const DerivedFromPlain>();
    threadsafe::assert_synchronizable<std::atomic<int> *const>();
    threadsafe::assert_synchronizable<const Plain[2]>();
    return true;
}

consteval bool lifetime_aware_diagnostics_agree() {
    threadsafe::assert_lifetime_aware<int>();
    threadsafe::assert_lifetime_aware<Plain>();
    threadsafe::assert_lifetime_aware<std::shared_ptr<int>>();
    threadsafe::assert_lifetime_aware<DerivedFromPlain>();
    threadsafe::assert_lifetime_aware<void (*)()>();
    threadsafe::assert_lifetime_aware<Plain[2]>();
    return true;
}

static_assert(sendable_diagnostics_agree());
static_assert(synchronizable_diagnostics_agree());
static_assert(lifetime_aware_diagnostics_agree());

// The other half of the contract: a trait that answers false must stay a plain
// false — the std::meta::exception thrown to carry the reason never escapes.
struct Borrowing {
    int *borrowed;
};

static_assert(!threadsafe::is_sendable_v<Borrowing>);
static_assert(!threadsafe::is_synchronizable_v<const Borrowing>);
static_assert(!threadsafe::is_lifetime_aware_v<Borrowing>);

// Nesting is where the walk now descends to name the root cause. The trait must
// still answer a plain false: the path that assert_* carries is what turns the
// descent on, and the trait never seeds one.
struct BorrowingMiddle {
    Borrowing inner;
};

struct BorrowingOuter {
    BorrowingMiddle middle;
};

static_assert(!threadsafe::is_sendable_v<BorrowingOuter>);
static_assert(!threadsafe::is_synchronizable_v<const BorrowingOuter>);
static_assert(!threadsafe::is_lifetime_aware_v<BorrowingOuter>);

}
