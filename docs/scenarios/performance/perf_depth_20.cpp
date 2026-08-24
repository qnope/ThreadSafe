#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Level0 { int value; };
struct Level1 { Level0 inner; int extra; };
struct Level2 { Level1 inner; int extra; };
struct Level3 { Level2 inner; int extra; };
struct Level4 { Level3 inner; int extra; };
struct Level5 { Level4 inner; int extra; };
struct Level6 { Level5 inner; int extra; };
struct Level7 { Level6 inner; int extra; };
struct Level8 { Level7 inner; int extra; };
struct Level9 { Level8 inner; int extra; };
struct Level10 { Level9 inner; int extra; };
struct Level11 { Level10 inner; int extra; };
struct Level12 { Level11 inner; int extra; };
struct Level13 { Level12 inner; int extra; };
struct Level14 { Level13 inner; int extra; };
struct Level15 { Level14 inner; int extra; };
struct Level16 { Level15 inner; int extra; };
struct Level17 { Level16 inner; int extra; };
struct Level18 { Level17 inner; int extra; };
struct Level19 { Level18 inner; int extra; };
struct Level20 { Level19 inner; int extra; };
static_assert(is_sendable_v<Level20>);
int main(){}
