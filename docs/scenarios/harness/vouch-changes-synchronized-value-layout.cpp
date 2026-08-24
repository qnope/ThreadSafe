#pragma once
#include <threadsafe/threadsafe.h>
#include <memory>

struct Widget { mutable int cache; int value; };

using Box = threadsafe::synchronized_value<Widget>;

std::shared_ptr<Box> make_in_tu_a();
void poke_in_tu_b(const std::shared_ptr<Box>&);
void report_a();
void report_b();
#include "sync_uq_odr_shared.h"
#include <cstdio>

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Widget);

std::shared_ptr<Box> make_in_tu_a() { return Box::make(Widget{0, 7}); }

void report_a() {
    std::printf("TU A: is_synchronizable_v<Widget>=%d sizeof(Box)=%zu mutex=%s\n",
                threadsafe::is_synchronizable_v<Widget>, sizeof(Box),
                std::meta::display_string_of(^^Box::mutex).data());
}
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
#include "sync_uq_odr_shared.h"
#include <cstdio>

int main() {
    report_a();
    report_b();
    auto box = make_in_tu_a();
    poke_in_tu_b(box);
    std::printf("survived\n");
}
