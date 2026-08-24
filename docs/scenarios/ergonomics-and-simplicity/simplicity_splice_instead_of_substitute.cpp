// Can the `trait_value(^^is_sendable_v, type)` indirection be replaced by a
// direct splice of the parameter?  This is the first question a conference
// audience asks.
#include <meta>
#include <type_traits>

template <class T>
constexpr bool is_sendable_v = std::is_scalar_v<T>;

consteval bool naive_trait_value(std::meta::info type) {
    return is_sendable_v<[:type:]>;
}

static_assert(naive_trait_value(^^int));
int main() {}
