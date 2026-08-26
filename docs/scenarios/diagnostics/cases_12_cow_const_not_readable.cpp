#include <threadsafe/threadsafe.h>

// Sendable (it owns its int), but a const read is not safe: the mutable member
// is written through a const reference.
struct MutableCounter {
    int value;
    mutable int cached;
};

int main() {
    threadsafe::assert_sendable<threadsafe::copy_on_write<MutableCounter>>();
}
