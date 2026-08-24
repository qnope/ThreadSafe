#include <threadsafe/threadsafe.h>

void free_function() {}

struct HoldsFunctionReference {
    void (&callback)();
};

using threadsafe::is_lifetime_aware_v;

static_assert(is_lifetime_aware_v<void (*)()>,
              "a function pointer has static storage duration");
static_assert(is_lifetime_aware_v<void (&)()>,
              "so does a function reference");
static_assert(is_lifetime_aware_v<HoldsFunctionReference>,
              "and a struct holding one cannot dangle");
static_assert(!is_lifetime_aware_v<int &>,
              "an ordinary reference is still refused");

int main() { return 0; }
