#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Level0 { int value; };
struct Level1 {
    Level0 member_0;
    Level0 member_1;
    Level0 member_2;
    Level0 member_3;
    Level0 member_4;
    Level0 member_5;
    Level0 member_6;
    Level0 member_7;
    Level0 member_8;
    Level0 member_9;
};
struct Level2 {
    Level1 member_0;
    Level1 member_1;
    Level1 member_2;
    Level1 member_3;
    Level1 member_4;
    Level1 member_5;
    Level1 member_6;
    Level1 member_7;
    Level1 member_8;
    Level1 member_9;
};
static_assert(is_sendable_v<Level2>);
