#include <threadsafe/threadsafe.h>

using sync_int = threadsafe::synchronized_value<int>;

// A helper that reads the value its caller has locked. `operator*` on a const
// lvalue guard is the library's supported spelling, so this compiles.
int& peek(const sync_int::guard& locked) { return *locked; }

int main() {
    sync_int value{1};
    // The guard is a temporary bound to a reference parameter: it is destroyed
    // at this semicolon, so the mutex is already unlocked below.
    int& escaped = peek(value.lock());
    escaped = 42;
}
