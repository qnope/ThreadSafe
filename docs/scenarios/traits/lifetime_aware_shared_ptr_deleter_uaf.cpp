// is_lifetime_aware<std::shared_ptr<T>> ignores the deleter, although
// is_lifetime_aware<std::unique_ptr<T, D>> checks D. A shared_ptr built with a
// deleter that borrows therefore crosses to another thread with the library's
// blessing, and the borrow dies first.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

struct Pool {
    std::atomic<int> released{0};
    void release(std::atomic<int> *cell) {
        released.fetch_add(1);   // <-- touches the Pool
        delete cell;
    }
};

struct PoolDeleter {
    Pool *pool;                  // borrows
    void operator()(std::atomic<int> *cell) const { pool->release(cell); }
};

using Cell = std::atomic<int>;

// The library checks the deleter of a unique_ptr...
static_assert(!threadsafe::is_lifetime_aware_v<std::unique_ptr<Cell, PoolDeleter>>,
              "unique_ptr: the borrowing deleter is seen");
// ...but the very same deleter inside a shared_ptr is invisible.
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<Cell>>,
              "shared_ptr: the deleter is type erased, so the trait says yes");
static_assert(threadsafe::is_sendable_v<std::shared_ptr<Cell>>);
static_assert(threadsafe::launchable_task<
                  void (*)(std::shared_ptr<Cell>), std::shared_ptr<Cell>>,
              "the launcher accepts it");

int main() {
    Pool *pool = new Pool();     // stands in for a pool owned by this scope
    {
        threadsafe::asynchronous_task_launcher launcher;
        std::shared_ptr<Cell> handle(new Cell(1), PoolDeleter{pool});

        launcher.launch_task(
            +[](std::shared_ptr<Cell> owned) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                owned->fetch_add(1);
                // `owned` dies here: last reference, PoolDeleter runs.
            },
            handle);

        handle.reset();          // this thread drops its reference

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        delete pool;             // the pool goes away while the task still runs
        std::puts("pool destroyed, worker still holds the shared_ptr");
    }                            // launcher joins -> deleter fires on a dead Pool
    std::puts("done");
}
