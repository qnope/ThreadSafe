#include <threadsafe/threadsafe.h>
#include <atomic>

struct Stats {
    std::atomic<long> hits;
    std::atomic<long> misses;
};
struct Derived : Stats {};

static_assert(!threadsafe::is_synchronizable_v<Stats>);
static_assert(!threadsafe::is_synchronizable_v<Derived>);
static_assert(!threadsafe::is_sendable_v<Stats&>);
static_assert(!threadsafe::is_sendable_v<std::shared_ptr<Stats>>);
static_assert(threadsafe::is_synchronizable_v<const Stats>);
static_assert(threadsafe::is_synchronizable_v<const Derived>);
int main() {}
