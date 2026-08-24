#include <threadsafe/threadsafe.h>
#include <chrono>

consteval bool explain() {
    threadsafe::assert_sendable<std::chrono::milliseconds>();
    return true;
}

static_assert(explain());
