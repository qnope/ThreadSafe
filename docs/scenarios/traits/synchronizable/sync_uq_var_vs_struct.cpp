#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstdio>

namespace { struct Widget { int* p; }; }

// CLAUDE.md documents the pair as "the shape of std::is_same / std::is_same_v".
// std::is_same_v is a variable template, and specializing a variable template is
// legal C++. Do exactly that, and only the reflective face notices.
template <>
constexpr bool threadsafe::is_synchronizable_v<Widget> = true;

int main() {
    std::printf("is_synchronizable_v<Widget>          = %d\n", threadsafe::is_synchronizable_v<Widget>);
    std::printf("is_synchronizable<Widget>::value     = %d\n", threadsafe::is_synchronizable<Widget>::value);
    std::printf("is_synchronizable_type(^^Widget)     = %d\n", threadsafe::is_synchronizable_type(^^Widget));
    std::printf("is_synchronizable_v<Widget[4]>       = %d\n", threadsafe::is_synchronizable_v<Widget[4]>);
    std::printf("is_synchronizable_v<std::atomic<Widget>> = %d\n", threadsafe::is_synchronizable_v<std::atomic<Widget>>);
    std::printf("is_sendable_v<Widget*>               = %d\n", threadsafe::is_sendable_v<Widget*>);
    std::printf("is_sendable_v<Widget>                = %d\n", threadsafe::is_sendable_v<Widget>);
}
