#include <threadsafe/threadsafe.h>
#include <atomic>
struct Rotted { std::atomic<long> hits; long backlog; };
consteval void why() { threadsafe::assert_synchronizable_members<Rotted>(); }
static_assert((why(), true));
int main() {}
