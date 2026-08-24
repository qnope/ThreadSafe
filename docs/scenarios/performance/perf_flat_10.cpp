#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Flat {
    int member_0;
    int member_1;
    int member_2;
    int member_3;
    int member_4;
    int member_5;
    int member_6;
    int member_7;
    int member_8;
    int member_9;
};
static_assert(is_sendable_v<Flat>);
static_assert(is_synchronizable_v<const Flat>);
static_assert(is_lifetime_aware_v<Flat>);
