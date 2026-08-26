#include <threadsafe/threadsafe.h>
#include <atomic>
static_assert((threadsafe::assert_sendable<std::atomic_ref<int>>(), true));
int main() {}
