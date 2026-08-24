#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Leaf { int value; };
struct Wide {
};
static_assert(is_sendable_v<Wide>);
int main(){}
