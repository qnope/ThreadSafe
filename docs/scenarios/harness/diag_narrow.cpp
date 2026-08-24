#include <threadsafe/threadsafe.h>
#include <ranges>
#include <vector>

namespace {
std::vector<int> *registry_slot = nullptr;
struct handle_view {
    std::size_t first = 0, last = 0;
    int *begin() const { return registry_slot->data() + first; }
    int *end() const { return registry_slot->data() + last; }
};
}
template <>
inline constexpr bool std::ranges::enable_borrowed_range<handle_view> = true;

consteval bool probe() { threadsafe::assert_lifetime_aware<handle_view>(); return true; }
static_assert(probe());
