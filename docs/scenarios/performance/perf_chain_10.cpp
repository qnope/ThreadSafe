#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Level0 { int value; };
struct Level1 { Level0 inner; };
struct Level2 { Level1 inner; };
struct Level3 { Level2 inner; };
struct Level4 { Level3 inner; };
struct Level5 { Level4 inner; };
struct Level6 { Level5 inner; };
struct Level7 { Level6 inner; };
struct Level8 { Level7 inner; };
struct Level9 { Level8 inner; };
struct Level10 { Level9 inner; };
static_assert(is_sendable_v<Level10>);
static_assert(is_synchronizable_v<const Level10>);
static_assert(is_lifetime_aware_v<Level10>);
