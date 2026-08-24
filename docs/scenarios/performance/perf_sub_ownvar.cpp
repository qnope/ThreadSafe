#include <threadsafe/threadsafe.h>
template <class T>
constexpr bool trivial_yes_v = true;
consteval int spin(int iterations) {
    int accumulator = 0;
    for (int index = 0; index < iterations; ++index)
        accumulator += std::meta::extract<bool>(
            std::meta::substitute(^^trivial_yes_v, {^^int})) ? 1 : 0;
    return accumulator;
}
static_assert(spin(50000) >= 0);
