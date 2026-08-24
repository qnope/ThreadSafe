#include "plain_odr.h"
#include <cstdio>
template <> struct my_is_sync<int> : std::true_type {};
std::shared_ptr<MyBox<int>> make_a() { return std::make_shared<MyBox<int>>(7); }
void report_a() { std::printf("TU A: sizeof(MyBox<int>)=%zu\n", sizeof(MyBox<int>)); }
