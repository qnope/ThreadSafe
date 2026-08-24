#include "sync_uq_odr_shared.h"
#include <cstdio>
#include <cstring>

void report_b() {
    std::printf("TU B: is_synchronizable_v<Widget>=%d sizeof(Box)=%zu mutex=%s\n",
                threadsafe::is_synchronizable_v<Widget>, sizeof(Box),
                std::meta::display_string_of(^^Box::mutex).data());
}

void poke_in_tu_b(const std::shared_ptr<Box>& box) {
    auto guard = box->lock();
    guard->value = 0x5A5A5A5A;
}
