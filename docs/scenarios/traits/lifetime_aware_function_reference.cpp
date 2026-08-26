// A function pointer is lifetime aware (functions have static storage
// duration). A function *reference* denotes exactly the same function, but the
// generic `T&` rule fires first and calls it a borrow.
#include <threadsafe/threadsafe.h>

void target() {}

static_assert(threadsafe::is_lifetime_aware_v<void (*)()>,
              "the carve-out: a pointer to a function keeps nothing alive "
              "because a function is never destroyed");
static_assert(threadsafe::is_lifetime_aware_v<void (*)() noexcept>);
static_assert(threadsafe::is_lifetime_aware_v<void (*[4])()>);
static_assert(threadsafe::is_lifetime_aware_v<void ()>,
              "even the function type itself answers yes");

static_assert(!threadsafe::is_lifetime_aware_v<void (&)()>,
              "OBSERVED: but a reference to that same function answers no");

static_assert(threadsafe::is_sendable_v<void (&)()>,
              "is_sendable does not have this asymmetry: a function type is "
              "synchronizable, so T& is sendable");

consteval void explain() { threadsafe::assert_lifetime_aware<void (&)()>(); }
static_assert((explain(), true));

int main() { auto &reference = target; reference(); }
