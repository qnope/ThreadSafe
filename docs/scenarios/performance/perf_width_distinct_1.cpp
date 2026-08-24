#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Leaf0 { int value; };
struct Wide {
    Leaf0 member0;
};
static_assert(is_sendable_v<Wide>);
int main(){}
