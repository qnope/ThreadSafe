#include "plain_odr.h"
#include <cstdio>
int main() { report_a(); report_b(); auto b = make_a(); poke_b(b); std::printf("survived\n"); }
