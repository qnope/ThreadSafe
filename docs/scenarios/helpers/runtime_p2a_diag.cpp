#include <threadsafe/threadsafe.h>
#include <functional>
#include <string>

struct Config { std::string name; };

// Both polarities pinned: the specialization is what answers, and it answers no
// because Config is not synchronizable -- nothing to do with copy/move members.
static_assert(!threadsafe::is_sendable_v<std::reference_wrapper<Config>>);
static_assert(!threadsafe::is_synchronizable_v<Config>);
static_assert(threadsafe::is_sendable_v<std::reference_wrapper<std::atomic<int>>>);
static_assert(threadsafe::is_synchronizable_v<std::atomic<int>>);

int main() { threadsafe::assert_sendable<std::reference_wrapper<Config>>(); }
