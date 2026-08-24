#include <threadsafe/threadsafe.h>

consteval bool explain() {
    threadsafe::assert_lifetime_aware<void (&)()>();
    return true;
}

static_assert(explain());

int main() { return 0; }
