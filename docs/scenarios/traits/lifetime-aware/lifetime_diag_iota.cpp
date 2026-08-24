#include <threadsafe/threadsafe.h>

#include <ranges>

consteval bool explain() {
    threadsafe::assert_lifetime_aware<std::ranges::iota_view<int, int>>();
    return true;
}

static_assert(explain());

int main() { return 0; }
