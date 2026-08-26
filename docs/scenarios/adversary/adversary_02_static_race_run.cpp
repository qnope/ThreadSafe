#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <thread>
#include <type_traits>
#include <vector>

class LookupTable {
public:
    int find(int key) const {
        ++probe_count_;
        return key * 2;
    }
    static long probes() { return probe_count_; }

private:
    static inline long probe_count_ = 0;
};

static_assert(threadsafe::is_sendable_v<LookupTable>);
static_assert(threadsafe::is_synchronizable_v<const LookupTable>);
static_assert(std::is_same_v<threadsafe::synchronized_value<LookupTable>::mutex,
                             std::shared_mutex>,
              "the library chose a shared_mutex: readers run concurrently");

int main() {
    constexpr long per_thread = 200000;
    constexpr int thread_count = 4;

    threadsafe::synchronized_value<LookupTable> table;

    std::vector<std::jthread> workers;
    for (int i = 0; i < thread_count; ++i)
        workers.emplace_back([&table] {
            for (long n = 0; n < per_thread; ++n) {
                auto reader = table.lock_shared();   // shared_lock: concurrent
                (void)reader->find(static_cast<int>(n));
            }
        });
    workers.clear();

    std::printf("expected %ld probes, observed %ld\n",
                per_thread * thread_count, LookupTable::probes());
    return LookupTable::probes() == per_thread * thread_count ? 0 : 1;
}
