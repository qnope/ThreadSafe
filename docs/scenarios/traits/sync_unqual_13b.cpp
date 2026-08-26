#include <threadsafe/threadsafe.h>
#include <atomic>
#include <memory>
static_assert((threadsafe::assert_sendable<std::shared_ptr<std::atomic_flag>>(), true));
int main() {}
