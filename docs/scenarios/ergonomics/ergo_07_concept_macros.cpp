#include <threadsafe/threadsafe.h>
#include <string>
#include <vector>

// The fourth spelling, now symmetric with sendable / lifetime_aware.
static_assert(threadsafe::synchronizable<const std::vector<int>>);

namespace acme {
struct Handle {
    int descriptor;
    ~Handle() {}
};
struct Borrowed {
    int* target;
};
}

THREADSAFE_UNSAFE_ASSERT_SENDABLE(acme::Handle);
THREADSAFE_UNSAFE_ASSERT_LIFETIME_AWARE(acme::Borrowed);

static_assert(threadsafe::is_sendable_v<acme::Handle>);
static_assert(threadsafe::is_lifetime_aware_v<acme::Borrowed>);

// A constrained function reads better than a bare bool.
void consume(threadsafe::synchronizable auto&) {}
