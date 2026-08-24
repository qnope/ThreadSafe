#include <threadsafe/threadsafe.h>
consteval int spin(int iterations) {
    int accumulator = 0;
    for (int index = 0; index < iterations; ++index)
        accumulator += std::meta::is_scalar_type(^^int) ? 1 : 0;
    return accumulator;
}
static_assert(spin(200000) >= 0);
