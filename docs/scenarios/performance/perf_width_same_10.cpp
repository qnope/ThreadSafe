#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Leaf { int value; };
struct Wide {
    Leaf member0;
    Leaf member1;
    Leaf member2;
    Leaf member3;
    Leaf member4;
    Leaf member5;
    Leaf member6;
    Leaf member7;
    Leaf member8;
    Leaf member9;
};
static_assert(is_sendable_v<Wide>);
int main(){}
