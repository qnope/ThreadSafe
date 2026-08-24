#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <ranges>
#include <vector>

// A hand-written view that borrows through an INDEX, not a pointer: the
// structural walk sees two size_t and nothing to object to.
namespace {
std::vector<int> *registry_slot = nullptr;

struct handle_view {
    std::size_t first = 0;
    std::size_t last = 0;
    int *begin() const { return registry_slot->data() + first; }
    int *end() const { return registry_slot->data() + last; }
};
}

template <>
inline constexpr bool std::ranges::enable_borrowed_range<handle_view> = true;

static_assert(std::ranges::borrowed_range<handle_view>);

int main() {
    std::printf("handle_view lifetime_aware=%d sendable=%d\n",
                (int)threadsafe::is_lifetime_aware_v<handle_view>,
                (int)threadsafe::is_sendable_v<handle_view>);
    return 0;
}
