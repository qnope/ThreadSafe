#include <threadsafe/threadsafe.h>
#include <memory>
#include <string>
#include <vector>

// The trait says false because of the ELEMENT type -- an explicit
// is_synchronizable<const std::vector<T,A>> rule decided it.
static_assert(!threadsafe::is_synchronizable_v<const std::vector<int *>>);

consteval void explain() {
    threadsafe::assert_synchronizable<const std::vector<int *>>();
}
static_assert((explain(), true));
