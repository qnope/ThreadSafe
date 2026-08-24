#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Level0 { int value; };
struct Level1 { Level0 inner; int extra; };
static_assert(is_sendable_v<Level1>);
int main(){}
