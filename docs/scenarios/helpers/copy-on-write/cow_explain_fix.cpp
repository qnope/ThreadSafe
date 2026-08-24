#include <threadsafe/threadsafe.h>
#include <optional>
#include <string>

using threadsafe::copy_on_write;

namespace {
struct Cache { int raw; mutable std::optional<int> parsed; };

consteval bool why_cow_cache_is_not_sendable() {
    threadsafe::assert_sendable<copy_on_write<Cache>>();
    return true;
}
}

static_assert(why_cow_cache_is_not_sendable());
int main() {}
