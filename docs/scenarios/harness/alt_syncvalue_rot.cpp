#include <threadsafe/threadsafe.h>
#include <atomic>
#include <memory>
struct Stats {
    std::atomic<long> hits;
    std::atomic<long> misses;
    long backlog;         // the rot
};
using SV = threadsafe::synchronized_value<Stats>;
// synchronized_value still holds: the mutex covers the plain member too.
static_assert(threadsafe::is_synchronizable_v<SV>);
int main() {}
