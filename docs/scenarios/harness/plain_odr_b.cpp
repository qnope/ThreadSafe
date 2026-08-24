#include "plain_odr.h"
#include <cstdio>
void report_b() { std::printf("TU B: sizeof(MyBox<int>)=%zu\n", sizeof(MyBox<int>)); }
void poke_b(const std::shared_ptr<MyBox<int>>& b) { b->poke(); }
