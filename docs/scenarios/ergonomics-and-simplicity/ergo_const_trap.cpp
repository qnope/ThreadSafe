// The const specialization asks a different, weaker question than the primary
// -- so adding const turns a "no" into a "yes", which no other C++ trait does.
// And the "yes" it gives cannot be spent: no reference form accepts it.
#include <threadsafe/threadsafe.h>

#include <atomic>

struct settings {
    int retry_count;
    double timeout_seconds;
};

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(!is_synchronizable_v<settings>);
static_assert(is_synchronizable_v<const settings>,
              "adding const makes the answer YES -- the opposite of every "
              "other trait in the standard library");

static_assert(!is_sendable_v<const settings &>,
              "yet the yes above buys nothing: a const& is still refused");
static_assert(!is_sendable_v<const settings *>);
static_assert(!is_sendable_v<std::shared_ptr<const settings>>);

// std::is_copy_constructible, for contrast: const never adds an ability.
static_assert(std::is_copy_constructible_v<settings>
              == std::is_copy_constructible_v<const settings>);
