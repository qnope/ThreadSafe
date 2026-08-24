#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstdio>

// The single most common thread-safe struct there is.
struct Stats {
    std::atomic<long> hits;
    std::atomic<long> misses;
};
struct Derived : Stats { std::atomic<long> extra; };

// Every member is synchronizable, no user-written special member, no invisible
// state, and the implicit copy-assign is deleted because atomic's is.
static_assert(!std::is_copy_assignable_v<Stats>);
static_assert(threadsafe::is_synchronizable_v<std::atomic<long>>);

static_assert(!threadsafe::is_synchronizable_v<Stats>, "REJECTED today");
static_assert(!threadsafe::is_synchronizable_v<Derived>, "REJECTED today");
static_assert(!threadsafe::is_sendable_v<Stats&>, "so it cannot be shared");
static_assert(!threadsafe::is_sendable_v<std::shared_ptr<Stats>>, "nor via shared_ptr");

// The const form, in contrast, does exactly this structural walk and says yes.
static_assert(threadsafe::is_synchronizable_v<const Stats>);
static_assert(threadsafe::is_synchronizable_v<const Derived>);

int main() { std::printf("compiled\n"); }
