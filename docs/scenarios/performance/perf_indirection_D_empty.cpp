#include <threadsafe/threadsafe.h>
consteval int spin(int iterations) {
    int accumulator = 0;
    for (int index = 0; index < iterations; ++index)
        accumulator += index & 1;
    return accumulator;
}
static_assert(spin(200000) >= 0);
