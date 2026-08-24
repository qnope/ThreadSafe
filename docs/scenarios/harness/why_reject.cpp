#include <threadsafe/threadsafe.h>
#include <atomic>
struct Stats { std::atomic<long> hits; std::atomic<long> misses; };
consteval void why() { threadsafe::assert_synchronizable<Stats>(); }
static_assert((why(), true));
int main() {}
