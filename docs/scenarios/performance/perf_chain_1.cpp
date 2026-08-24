#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Level0 { int value; };
struct Level1 { Level0 inner; };
static_assert(is_sendable_v<Level1>);
static_assert(is_synchronizable_v<const Level1>);
static_assert(is_lifetime_aware_v<Level1>);
