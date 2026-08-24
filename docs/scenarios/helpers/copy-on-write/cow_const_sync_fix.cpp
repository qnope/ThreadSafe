// Verifies the proposed fix: adding is_synchronizable<const copy_on_write<T>>
// restores composition without making a non-const copy_on_write shareable.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <mutex>
#include <string>

namespace threadsafe {
template <class T>
struct is_synchronizable<const copy_on_write<T>> : is_synchronizable<const T> {};
}

using threadsafe::copy_on_write;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

namespace {
struct Cache {
    int raw;
    mutable std::optional<int> parsed;
};

struct DocumentWithCowMember {
    copy_on_write<std::string> body;
};
}

static_assert(is_synchronizable_v<const copy_on_write<std::string>>);
static_assert(!is_synchronizable_v<copy_on_write<std::string>>,
              "still not shareable by reference — as_mutable rebinds the handle");
static_assert(!is_synchronizable_v<const copy_on_write<Cache>>,
              "and the const rule still walks into the T");

static_assert(is_sendable_v<copy_on_write<copy_on_write<std::string>>>);
static_assert(is_sendable_v<copy_on_write<DocumentWithCowMember>>);
static_assert(!is_sendable_v<copy_on_write<copy_on_write<Cache>>>);

static_assert(std::same_as<threadsafe::synchronized_value<
                               copy_on_write<std::string>>::mutex,
                           std::shared_mutex>,
              "readers of a synchronized_value<cow<T>> share the lock again");

int main() {}
