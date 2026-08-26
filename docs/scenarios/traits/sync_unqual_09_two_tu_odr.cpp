// One file, compiled twice into two translation units:
//
//   g++-16 -std=c++26 -freflection -I<include> -DTHREADSAFE_TU_WITH_VOUCH -c \
//          -o with_vouch.o 09_two_tu_odr.cpp
//   g++-16 -std=c++26 -freflection -I<include> -c \
//          -o without_vouch.o 09_two_tu_odr.cpp
//   g++-16 with_vouch.o without_vouch.o -o 09 && ./09
//
// The two TUs disagree about threadsafe::is_synchronizable<const Cache> because
// only one of them saw the vouch. threadsafe::synchronized_value<Cache> picks
// its mutex member from that answer, so the same class has two different
// definitions -- and two different sizes -- in one program.

#include <threadsafe/threadsafe.h>

#include <cstdio>

struct Cache {
    mutable int hits;
};

#ifdef THREADSAFE_TU_WITH_VOUCH
// The vouch is written here, in this TU only -- exactly the pattern
// tests/test_deferred_specialization.cpp presents as supported.
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(const Cache);
#endif

using SharedCache = threadsafe::synchronized_value<Cache>;

void read_from_vouching_tu(SharedCache &shared_cache);
std::size_t size_seen_by_vouching_tu();

#ifdef THREADSAFE_TU_WITH_VOUCH

std::size_t size_seen_by_vouching_tu() { return sizeof(SharedCache); }

void read_from_vouching_tu(SharedCache &shared_cache) {
    // With the vouch, mutex is std::shared_mutex (200 bytes here) and this
    // takes a shared_lock over bytes the other TU never allocated.
    auto guard = shared_cache.lock_shared();
    std::printf("  vouching TU read hits = %d\n", guard->hits);
}

#else

struct Sandwich {
    SharedCache shared_cache;
    unsigned long canary;
};

int main() {
    std::printf("sizeof(synchronized_value<Cache>) in the plain TU    = %zu\n",
                sizeof(SharedCache));
    std::printf("sizeof(synchronized_value<Cache>) in the vouching TU = %zu\n",
                size_seen_by_vouching_tu());

    Sandwich sandwich{SharedCache{Cache{7}}, 0xC0FFEEUL};
    std::printf("canary before = 0x%lX\n", sandwich.canary);
    read_from_vouching_tu(sandwich.shared_cache);
    std::printf("canary after  = 0x%lX\n", sandwich.canary);
    if (sandwich.canary != 0xC0FFEEUL)
        std::printf("CANARY CLOBBERED: the two TUs disagree on the layout\n");
    return 0;
}

#endif
