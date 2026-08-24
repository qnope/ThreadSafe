// Asking the trait about a unique_ptr to an incomplete type -- the query a
// pimpl user makes first.
#include <threadsafe/threadsafe.h>

#include <memory>

namespace {
struct Implementation;
}

static_assert(!threadsafe::is_sendable_v<std::unique_ptr<Implementation>>);
int main() {}
