#include <threadsafe/threadsafe.h>
template <class T> using identity_of = T;
consteval int spin(int iterations) {
    int accumulator = 0;
    for (int index = 0; index < iterations; ++index)
        accumulator += std::meta::substitute(^^identity_of, {^^int}) == ^^int ? 1 : 0;
    return accumulator;
}
static_assert(spin(50000) >= 0);
