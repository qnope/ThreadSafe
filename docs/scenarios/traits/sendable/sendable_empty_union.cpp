#include <threadsafe/threadsafe.h>

union EmptyUnion {};

consteval bool explain_union() {
    threadsafe::assert_sendable<EmptyUnion>();
    return true;
}
static_assert(explain_union());
