#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Chain0_0 { int value; };
struct Chain0_1 { Chain0_0 inner; int extra; };
struct Chain0_2 { Chain0_1 inner; int extra; };
struct Chain0_3 { Chain0_2 inner; int extra; };
struct Chain0_4 { Chain0_3 inner; int extra; };
struct Chain0_5 { Chain0_4 inner; int extra; };
struct Chain0_6 { Chain0_5 inner; int extra; };
struct Chain0_7 { Chain0_6 inner; int extra; };
struct Chain0_8 { Chain0_7 inner; int extra; };
struct Chain0_9 { Chain0_8 inner; int extra; };
struct Chain0_10 { Chain0_9 inner; int extra; };
struct Chain0_11 { Chain0_10 inner; int extra; };
struct Chain0_12 { Chain0_11 inner; int extra; };
struct Chain0_13 { Chain0_12 inner; int extra; };
struct Chain0_14 { Chain0_13 inner; int extra; };
struct Chain0_15 { Chain0_14 inner; int extra; };
struct Chain0_16 { Chain0_15 inner; int extra; };
struct Chain0_17 { Chain0_16 inner; int extra; };
struct Chain0_18 { Chain0_17 inner; int extra; };
struct Chain0_19 { Chain0_18 inner; int extra; };
struct Chain0_20 { Chain0_19 inner; int extra; };
static_assert(is_sendable_v<Chain0_20>);
int main(){}
