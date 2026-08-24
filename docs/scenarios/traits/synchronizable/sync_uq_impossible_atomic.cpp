#include <threadsafe/threadsafe.h>
#include <atomic>
#include <string>
#include <vector>

// The trait answers about std::atomic<T> without ever asking whether such an
// atomic can exist.
static_assert(threadsafe::is_synchronizable_v<std::atomic<std::vector<int>>>);
static_assert(threadsafe::is_synchronizable_v<std::atomic<std::string>>);
static_assert(threadsafe::is_sendable_v<std::atomic<std::string>&>);

// ... and the type itself is ill-formed.
std::atomic<std::string> cannot_exist;
