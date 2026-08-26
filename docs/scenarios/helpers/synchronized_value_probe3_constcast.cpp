#include <threadsafe/threadsafe.h>
#include <mutex>
#include <shared_mutex>
#include <concepts>

// A lazily-initialised cache that does NOT use `mutable`: it launders the
// constness away with const_cast.  Reflection sees two plain ints.
struct LazySquare {
    int seed;
    int cache;
    bool cached;

    int square() const {
        if (!cached) {
            const_cast<LazySquare*>(this)->cache = seed * seed;
            const_cast<LazySquare*>(this)->cached = true;
        }
        return cache;
    }
};

// A const member function whose state lives in a function-local static.
struct HitCounter {
    int seed;
    int next() const {
        static int calls = 0;
        return seed + (++calls);
    }
};

using threadsafe::is_synchronizable_v;
using threadsafe::is_sendable_v;

static_assert(is_sendable_v<LazySquare>);
static_assert(is_synchronizable_v<const LazySquare>,
              "the trait says a const LazySquare is read-safe");
static_assert(is_sendable_v<HitCounter>);
static_assert(is_synchronizable_v<const HitCounter>,
              "the trait says a const HitCounter is read-safe");

using sync_lazy = threadsafe::synchronized_value<LazySquare>;
using sync_hits = threadsafe::synchronized_value<HitCounter>;

static_assert(std::same_as<sync_lazy::mutex, std::shared_mutex>,
              "a shared_mutex is selected");
static_assert(std::same_as<sync_lazy::const_guard,
                           threadsafe::value_guard<const LazySquare,
                                                   std::shared_lock<std::shared_mutex>>>,
              "readers really share");
static_assert(std::same_as<sync_hits::mutex, std::shared_mutex>);
static_assert(std::same_as<sync_hits::const_guard,
                           threadsafe::value_guard<const HitCounter,
                                                   std::shared_lock<std::shared_mutex>>>);
