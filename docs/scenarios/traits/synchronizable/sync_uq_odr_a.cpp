#include "sync_uq_odr_shared.h"
#include <cstdio>

// This translation unit vouches for Widget. Nothing here is visible to TU B.
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Widget);

std::shared_ptr<Box> make_in_tu_a() { return Box::make(Widget{0, 7}); }

void report_a() {
    std::printf("TU A: is_synchronizable_v<Widget>=%d sizeof(Box)=%zu mutex=%s\n",
                threadsafe::is_synchronizable_v<Widget>, sizeof(Box),
                std::meta::display_string_of(^^Box::mutex).data());
}
