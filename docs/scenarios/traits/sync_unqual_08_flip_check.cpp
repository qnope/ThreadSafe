#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <mutex>
#include <shared_mutex>

struct Cache { mutable int hits; };

#ifdef VOUCH
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(const Cache);
#endif

static_assert(threadsafe::is_sendable_v<Cache>);

int main() {
    std::printf("sendable<Cache>            = %d\n", (int)threadsafe::is_sendable_v<Cache>);
    std::printf("synchronizable<const Cache>= %d\n", (int)threadsafe::is_synchronizable_v<const Cache>);
    std::printf("sizeof synchronized_value  = %zu\n", sizeof(threadsafe::synchronized_value<Cache>));
    std::printf("sizeof std::mutex          = %zu\n", sizeof(std::mutex));
    std::printf("sizeof std::shared_mutex   = %zu\n", sizeof(std::shared_mutex));
}
