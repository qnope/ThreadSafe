#include <threadsafe/threadsafe.h>
#include <vector>
using sync_vec = threadsafe::synchronized_value<std::vector<int>>;
int main() {
    auto shared = sync_vec::make();
    int total = 0;
    // C++23 P2718R0 extends the lifetime of temporaries in the range-init to the
    // whole loop, so the lock WOULD still be held. The deletion rejects it anyway.
    for (int element : *shared->lock()) total += element;
    return total;
}
