#include <threadsafe/threadsafe.h>
namespace { struct WithZero { int z[0]; int v; }; }
static_assert((threadsafe::assert_sendable<WithZero>(), true));
