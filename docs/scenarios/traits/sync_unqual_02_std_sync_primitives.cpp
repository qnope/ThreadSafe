#include <threadsafe/threadsafe.h>

#include <atomic>
#include <barrier>
#include <latch>
#include <mutex>
#include <semaphore>
#include <shared_mutex>

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

#define PROBE(EXPR, NAME) constexpr bool NAME = (EXPR)

PROBE(is_synchronizable_v<std::atomic<int>>, sync_atomic_int);
PROBE(is_synchronizable_v<std::atomic_flag>, sync_atomic_flag);
PROBE(is_sendable_v<std::atomic_flag>, send_atomic_flag);
PROBE(is_synchronizable_v<const std::atomic_flag>, sync_const_atomic_flag);
PROBE(is_synchronizable_v<std::atomic_ref<int>>, sync_atomic_ref);
PROBE(is_sendable_v<std::atomic_ref<int>>, send_atomic_ref);
PROBE(is_synchronizable_v<const std::atomic_ref<int>>, sync_const_atomic_ref);
PROBE(is_synchronizable_v<std::mutex>, sync_mutex);
PROBE(is_sendable_v<std::mutex>, send_mutex);
PROBE(is_synchronizable_v<std::shared_mutex>, sync_shared_mutex);
PROBE(is_synchronizable_v<std::latch>, sync_latch);
PROBE(is_synchronizable_v<std::barrier<>>, sync_barrier);
PROBE(is_synchronizable_v<std::counting_semaphore<8>>, sync_semaphore);
PROBE(is_synchronizable_v<volatile std::atomic<int>>, sync_volatile_atomic);
PROBE(is_synchronizable_v<const volatile std::atomic<int>>, sync_const_volatile_atomic);
PROBE(is_synchronizable_v<std::atomic<int> volatile[4]>, sync_volatile_atomic_array);

#include <cstdio>
int main() {
#define SHOW(NAME) std::printf("%-32s = %s\n", #NAME, NAME ? "true" : "false")
    SHOW(sync_atomic_int);
    SHOW(sync_atomic_flag);
    SHOW(send_atomic_flag);
    SHOW(sync_const_atomic_flag);
    SHOW(sync_atomic_ref);
    SHOW(send_atomic_ref);
    SHOW(sync_const_atomic_ref);
    SHOW(sync_mutex);
    SHOW(send_mutex);
    SHOW(sync_shared_mutex);
    SHOW(sync_latch);
    SHOW(sync_barrier);
    SHOW(sync_semaphore);
    SHOW(sync_volatile_atomic);
    SHOW(sync_const_volatile_atomic);
    SHOW(sync_volatile_atomic_array);
}
