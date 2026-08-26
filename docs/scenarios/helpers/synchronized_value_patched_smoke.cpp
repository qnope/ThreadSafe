#include <threadsafe/threadsafe.h>
#include <concepts>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

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

struct Memo { int key; mutable int cached; };

using threadsafe::synchronized_value;

// scalars and standard containers keep their shared_mutex
static_assert(std::same_as<synchronized_value<int>::mutex, std::shared_mutex>);
static_assert(std::same_as<synchronized_value<std::vector<int>>::mutex,
                           std::shared_mutex>);
static_assert(std::same_as<synchronized_value<std::string>::mutex,
                           std::shared_mutex>);
// a user class no longer gets one for free
static_assert(std::same_as<synchronized_value<LazySquare>::mutex, std::mutex>);
static_assert(std::same_as<synchronized_value<std::vector<LazySquare>>::mutex,
                           std::mutex>);
static_assert(std::same_as<synchronized_value<Memo>::mutex, std::mutex>);
static_assert(std::same_as<
    synchronized_value<LazySquare>::const_guard,
    threadsafe::value_guard<const LazySquare, std::unique_lock<std::mutex>>>);
