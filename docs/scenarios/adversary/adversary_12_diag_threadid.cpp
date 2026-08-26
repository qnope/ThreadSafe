#include <threadsafe/threadsafe.h>
#include <thread>
static_assert((threadsafe::assert_sendable<std::thread::id>(), true));
