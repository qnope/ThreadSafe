#include <threadsafe/threadsafe.h>
using sync_int = threadsafe::synchronized_value<int>;
int main() {
    auto shared = sync_int::make(0);
    const auto guard = shared->lock();   // SAFE: named, lives to end of scope
    return *guard;
}
