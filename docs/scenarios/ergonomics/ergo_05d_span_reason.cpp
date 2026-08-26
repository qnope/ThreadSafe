#include <threadsafe/threadsafe.h>
#include <span>
consteval bool probe() {
    threadsafe::assert_lifetime_aware<std::span<const int>>();
    return true;
}
static_assert(probe());
