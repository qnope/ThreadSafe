#include <threadsafe/threadsafe.h>
consteval int spin(int iterations) {
    int accumulator = 0;
    for (int index = 0; index < iterations; ++index)
        accumulator += threadsafe::detail::trait_value(
                           ^^threadsafe::is_sendable_v, ^^int) ? 1 : 0;
    return accumulator;
}
static_assert(spin(200000) >= 0);
