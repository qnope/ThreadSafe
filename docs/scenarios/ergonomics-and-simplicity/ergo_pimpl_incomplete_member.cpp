// Same pimpl, but written the way a header-only wrapper often is: the special
// members are defaulted out of line, so the structural walk reaches the
// unique_ptr member and asks about the incomplete implementation type.
#include <threadsafe/threadsafe.h>

#include <memory>

namespace app {
struct implementation;
}

static_assert(!threadsafe::is_sendable_v<std::unique_ptr<app::implementation>>);
