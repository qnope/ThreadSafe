#include <threadsafe/threadsafe.h>

// A device-register layout: reserved bits, no named member. Not a closure.
struct ReservedRegister {
    unsigned : 32;
};

consteval bool explain_register() {
    threadsafe::assert_sendable<ReservedRegister>();
    return true;
}
static_assert(explain_register());
