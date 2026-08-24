#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Level0 { int value; };
struct Level1 { Level0 inner; int extra; };
struct Level2 { Level1 inner; int extra; };
struct Level3 { Level2 inner; int extra; };
struct Level4 { Level3 inner; int extra; };
struct Level5 { Level4 inner; int extra; };
static_assert(is_sendable_v<Level5>);
int main(){}
