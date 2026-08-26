#include <threadsafe/threadsafe.h>

namespace {
struct Registry {};
}

static_assert(!threadsafe::is_synchronizable_v<Registry>);

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Registry);

static_assert(threadsafe::is_synchronizable_v<Registry>);

int main() {}
