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
static_assert(is_sendable_v<Level10>);
int main(){}
