#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Leaf0 { int value; };
struct Leaf1 { int value; };
struct Leaf2 { int value; };
struct Leaf3 { int value; };
struct Leaf4 { int value; };
struct Leaf5 { int value; };
struct Leaf6 { int value; };
struct Leaf7 { int value; };
struct Leaf8 { int value; };
struct Leaf9 { int value; };
struct Wide {
    Leaf0 member0;
    Leaf1 member1;
    Leaf2 member2;
    Leaf3 member3;
    Leaf4 member4;
    Leaf5 member5;
    Leaf6 member6;
    Leaf7 member7;
    Leaf8 member8;
    Leaf9 member9;
};
static_assert(is_sendable_v<Wide>);
int main(){}
