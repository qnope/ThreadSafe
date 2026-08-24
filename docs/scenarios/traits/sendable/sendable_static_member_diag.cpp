#include <threadsafe/threadsafe.h>

struct RequestCounter {
    static inline long long total = 0;
    int weight = 1;
};

consteval bool explain() {
    threadsafe::assert_sendable<RequestCounter>();
    return true;
}
static_assert(explain());
