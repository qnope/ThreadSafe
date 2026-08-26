// A std::shared_ptr built through the aliasing constructor with an empty
// control block owns nothing at all. is_lifetime_aware<std::shared_ptr<T>> is
// answered from the static type alone, so the launcher accepts it.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

using Cell = std::atomic<int>;

static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<Cell>>,
              "the trait reads the static type: a shared_ptr owns its pointee");
static_assert(threadsafe::launchable_task<void (*)(std::shared_ptr<Cell>),
                                          std::shared_ptr<Cell>>,
              "so the launcher lets it cross");

int main() {
    {
        threadsafe::asynchronous_task_launcher launcher;
        std::vector<Cell> cells(4);

        // [util.smartptr.shared.const]: the aliasing constructor with an empty
        // shared_ptr yields a shared_ptr with no ownership at all.
        std::shared_ptr<Cell> borrowed(std::shared_ptr<Cell>{}, cells.data());
        std::fprintf(stderr, "use_count = %ld\n", (long) borrowed.use_count());

        launcher.launch_task(
            +[](std::shared_ptr<Cell> owned) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                owned->fetch_add(1);        // writes into cells[0]
            },
            borrowed);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        cells.clear();
        cells.shrink_to_fit();              // the storage is gone
        std::fputs("cells released, worker still holds the shared_ptr\n", stderr);
    }
    std::puts("done");
}
