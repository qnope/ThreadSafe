#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>

namespace {
struct Vouched {};
struct HoldsRawPointerToVouched { Vouched* target; };
struct BigPayload { long a, b, c, d, e, f, g, h; };
struct HasMutableCounter { mutable int hits; };
struct HasReferenceMember { int& referent; };
}

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Vouched);

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

#define PROBE(EXPR, NAME) constexpr bool NAME = (EXPR)

PROBE(is_synchronizable_v<std::atomic<BigPayload>>, sync_atomic_big);
PROBE(is_synchronizable_v<std::atomic<HoldsRawPointerToVouched>>, sync_atomic_ptr_struct);
PROBE(is_synchronizable_v<std::atomic<HasMutableCounter>>, sync_atomic_mutable);
PROBE(is_synchronizable_v<std::atomic<std::string>>, sync_atomic_string);
PROBE(is_synchronizable_v<std::atomic<std::shared_ptr<int>>>, sync_atomic_sptr_int);
PROBE(is_synchronizable_v<std::atomic<std::shared_ptr<std::atomic<int>>>>, sync_atomic_sptr_atomic);
PROBE(is_synchronizable_v<std::atomic<std::atomic<int>>>, sync_atomic_atomic);
PROBE(is_synchronizable_v<std::atomic<std::mutex>>, sync_atomic_mutex);
PROBE(is_synchronizable_v<std::atomic<HasReferenceMember>>, sync_atomic_refmember);
PROBE(is_synchronizable_v<std::atomic<void*>>, sync_atomic_voidptr);
PROBE(is_synchronizable_v<std::atomic<int*>>, sync_atomic_intptr);
PROBE(is_synchronizable_v<std::atomic<Vouched*>>, sync_atomic_vouchedptr);
PROBE(is_sendable_v<std::atomic<BigPayload>*>, send_ptr_atomic_big);

int main() {
#define SHOW(NAME) std::printf("%-32s = %s\n", #NAME, NAME ? "true" : "false")
    SHOW(sync_atomic_big);
    SHOW(sync_atomic_ptr_struct);
    SHOW(sync_atomic_mutable);
    SHOW(sync_atomic_string);
    SHOW(sync_atomic_sptr_int);
    SHOW(sync_atomic_sptr_atomic);
    SHOW(sync_atomic_atomic);
    SHOW(sync_atomic_mutex);
    SHOW(sync_atomic_refmember);
    SHOW(sync_atomic_voidptr);
    SHOW(sync_atomic_intptr);
    SHOW(sync_atomic_vouchedptr);
    SHOW(send_ptr_atomic_big);
}
