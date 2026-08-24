#include <threadsafe/threadsafe.h>
#include <atomic>
#include <memory>

// Same idiom, but somebody added the plain member later.
struct Stats {
    mutable std::atomic<long> hits;
    mutable std::atomic<long> misses;
    mutable long backlog;     // rot
};

// The trait NOTICES: no vouch to launder it.
static_assert(!threadsafe::is_synchronizable_v<const Stats>);
static_assert(!threadsafe::is_sendable_v<std::shared_ptr<const Stats>>);

consteval void why() { threadsafe::assert_synchronizable<const Stats>(); }
static_assert((why(), true));
int main() {}
