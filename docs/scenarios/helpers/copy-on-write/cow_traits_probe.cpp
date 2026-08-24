#include <threadsafe/threadsafe.h>

#include <atomic>
#include <concepts>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

using threadsafe::copy_on_write;
using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

namespace {
struct SyncCache {
    mutable std::atomic<int> hits;
};

struct DocumentWithCowMember {
    copy_on_write<std::string> body;
};
}

// ---- 1. const copy_on_write is never synchronizable, for ANY T -------------
static_assert(!is_synchronizable_v<const copy_on_write<int>>);
static_assert(!is_synchronizable_v<const copy_on_write<std::string>>);
static_assert(!is_synchronizable_v<const copy_on_write<SyncCache>>);

// ---- 2. consequence: copy_on_write does not compose with itself ------------
static_assert(is_sendable_v<copy_on_write<std::string>>);
static_assert(!is_sendable_v<copy_on_write<copy_on_write<std::string>>>);

// ---- 3. consequence: a struct holding a cow cannot be put in a cow ---------
static_assert(is_sendable_v<DocumentWithCowMember>);
static_assert(!is_sendable_v<copy_on_write<DocumentWithCowMember>>);

// ---- 4. synchronized_value degrades from shared_mutex to mutex -------------
static_assert(std::same_as<threadsafe::synchronized_value<
                               copy_on_write<std::string>>::mutex,
                           std::mutex>,
              "readers of a synchronized_value<cow<T>> cannot share the lock");

int main() {}
