#include <meta>
#include <type_traits>
#include <vector>

template <class T> struct Trait;

consteval bool specialization_is_complete(std::meta::info type) {
    return std::meta::is_complete_type(std::meta::substitute(^^Trait, std::vector{type}));
}

struct Leaf {};
struct Cycle;

consteval bool compute(std::meta::info type) {
    if (type == ^^Cycle) {
        // ask for our own specialization while it is being instantiated
        return specialization_is_complete(^^Cycle);
    }
    return true;
}

template <class T> struct Trait : std::bool_constant<compute(^^T)> {};

static_assert(Trait<Leaf>::value, "leaf false");
static_assert(!Trait<Cycle>::value, "self-query reported COMPLETE");
