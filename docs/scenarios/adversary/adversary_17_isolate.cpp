#include <threadsafe/threadsafe.h>
struct OnlyInt { static constexpr int limit = 4; int payload; };
static_assert(threadsafe::is_sendable_v<OnlyInt>, "static constexpr int");
static_assert(threadsafe::is_synchronizable_v<const OnlyInt>, "static constexpr int const-sync");
struct WithEnum { static constexpr double ratio = 0.5; static constexpr char tag = 'x'; int payload; };
static_assert(threadsafe::is_sendable_v<WithEnum>, "static constexpr scalars");
struct WithArray { static constexpr int table[3] = {1,2,3}; int payload; };
static_assert(threadsafe::is_sendable_v<WithArray>, "static constexpr array");
