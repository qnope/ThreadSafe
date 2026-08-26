#include <meta>
#include <type_traits>
#include <vector>

template <class T> struct Trait;
template <class T> constexpr bool Trait_v = Trait<T>::value;

struct Leaf {};
struct Cycle;

// exactly the library's shape, plus a completeness guard on the CLASS template
consteval bool trait_value_guarded(std::meta::info class_template,
                                   std::meta::info variable_template,
                                   std::meta::info type) {
    if (!std::meta::is_complete_type(std::meta::substitute(class_template, std::vector{type})))
        return true;   // coinductive: a cycle adds no new obligation
    return std::meta::extract<bool>(std::meta::substitute(variable_template, std::vector{type}));
}

consteval bool compute(std::meta::info type) {
    if (type == ^^Cycle)
        return trait_value_guarded(^^Trait, ^^Trait_v, ^^Cycle);
    return true;
}

template <class T> struct Trait : std::bool_constant<compute(^^T)> {};

static_assert(Trait_v<Leaf>, "leaf false");
static_assert(Trait_v<Cycle>, "cycle guard failed");
