#include <threadsafe/threadsafe.h>
#include <barrier>
#include <cstdio>
#include <thread>
#include <vector>

struct LazyTable {
    std::vector<int> rows;
    bool ready = false;

    const std::vector<int>& get() const {
        if (!ready) {
            auto& self = *const_cast<LazyTable*>(this);
            self.rows.assign(4096, 7);
            self.ready = true;
        }
        return rows;
    }
};

static_assert(threadsafe::is_sendable_v<LazyTable>);
static_assert(threadsafe::is_synchronizable_v<const LazyTable>,
              "the library says a const LazyTable is readable from several "
              "threads at once");
static_assert(std::is_same_v<threadsafe::synchronized_value<LazyTable>::mutex,
                             std::shared_mutex>);

int main() {
    constexpr int worker_count = 4;
    long bad = 0;
    for (int round = 0; round < 2000 && bad == 0; ++round) {
        threadsafe::synchronized_value<LazyTable> table;
        std::barrier start{worker_count};
        {
            std::vector<std::jthread> workers;
            for (int i = 0; i < worker_count; ++i)
                workers.emplace_back([&table, &bad, &start] {
                    start.arrive_and_wait();
                    auto reader = table.lock_shared();   // shared_lock: concurrent
                    const auto& rows = reader->get();
                    for (int value : rows)
                        if (value != 7)
                            __atomic_fetch_add(&bad, 1, __ATOMIC_RELAXED);
                });
        }
    }
    std::printf("observed %ld corrupted elements\n", bad);
    return bad != 0;
}
