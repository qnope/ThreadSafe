#include "sync_uq_odr_shared.h"
#include <cstdio>

int main() {
    report_a();
    report_b();
    auto box = make_in_tu_a();
    poke_in_tu_b(box);
    std::printf("survived\n");
}
