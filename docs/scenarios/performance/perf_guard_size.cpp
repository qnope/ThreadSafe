// Q3: sizeof(guard) for both mutex policies, and what the stored T* costs.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

struct exclusive_only {                 // a mutable member kills const-synchronizable
    mutable int cache_ = 0;
    int value_ = 0;
};

using shared_form   = threadsafe::synchronized_value<std::vector<int>>;
using exclusive_form = threadsafe::synchronized_value<exclusive_only>;

static_assert(std::is_same_v<shared_form::mutex, std::shared_mutex>);
static_assert(std::is_same_v<exclusive_form::mutex, std::mutex>);

int main() {
    std::printf("--- shared_mutex form: synchronized_value<std::vector<int>> ---\n");
    std::printf("  sizeof(guard)                          = %2zu (align %zu)\n",
                sizeof(shared_form::guard), alignof(shared_form::guard));
    std::printf("  sizeof(const_guard)                    = %2zu (align %zu)\n",
                sizeof(shared_form::const_guard), alignof(shared_form::const_guard));
    std::printf("  sizeof(std::unique_lock<shared_mutex>) = %2zu\n",
                sizeof(std::unique_lock<std::shared_mutex>));
    std::printf("  sizeof(std::shared_lock<shared_mutex>) = %2zu\n",
                sizeof(std::shared_lock<std::shared_mutex>));

    std::printf("--- mutex form: synchronized_value<exclusive_only> ---\n");
    std::printf("  sizeof(guard)                          = %2zu (align %zu)\n",
                sizeof(exclusive_form::guard), alignof(exclusive_form::guard));
    std::printf("  sizeof(const_guard)                    = %2zu (align %zu)\n",
                sizeof(exclusive_form::const_guard), alignof(exclusive_form::const_guard));
    std::printf("  sizeof(std::unique_lock<std::mutex>)   = %2zu\n",
                sizeof(std::unique_lock<std::mutex>));

    std::printf("--- the object itself ---\n");
    std::printf("  sizeof(synchronized_value<vector<int>>) = %2zu  (shared_mutex %zu + vector %zu)\n",
                sizeof(shared_form), sizeof(std::shared_mutex), sizeof(std::vector<int>));
    std::printf("  sizeof(synchronized_value<exclusive_only>) = %2zu  (mutex %zu + T %zu)\n",
                sizeof(exclusive_form), sizeof(std::mutex), sizeof(exclusive_only));
}
