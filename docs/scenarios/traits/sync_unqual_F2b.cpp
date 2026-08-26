#include <threadsafe/threadsafe.h>
#include <mutex>
struct GuardedCounter { mutable std::mutex gate; int value; };
static_assert((threadsafe::assert_synchronizable<const GuardedCounter>(), true));
int main() {}
