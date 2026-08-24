// One translation unit, one type, two contradictory answers -- and the
// compiler is silent. The only difference is that the first question was asked
// before the vouch was written.
#include <threadsafe/threadsafe.h>

#include <memory>

namespace app {
struct device {
    int *memory_mapped_register;
};
}

// A user checks first, then vouches -- the natural order when exploring.
static_assert(!threadsafe::is_sendable_v<std::shared_ptr<app::device>>,
              "asked BEFORE the vouch: not sendable");

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(app::device);

static_assert(threadsafe::is_sendable_v<std::shared_ptr<app::device>>,
              "asked AFTER the vouch: sendable");

// Both assertions above hold in the same translation unit.
