#include <threadsafe/threadsafe.h>
#include <atomic>
static_assert((threadsafe::assert_synchronizable<std::atomic<int> &>(), true));
