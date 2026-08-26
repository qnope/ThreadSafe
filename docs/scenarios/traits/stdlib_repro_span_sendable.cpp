#include <threadsafe/threadsafe.h>

#include <span>
#include <vector>

namespace {
// A type the user has vouched for as synchronizing itself.
struct SyncCell {
    int value;
};
}
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncCell);

// A raw pointer to a synchronizable type is sendable ...
static_assert(threadsafe::is_sendable_v<SyncCell *>);

// ... but std::span<SyncCell>, which is exactly that pointer plus a length, is
// not -- and the reason the library gives has nothing to do with sharing.
static_assert(!threadsafe::is_sendable_v<std::span<SyncCell>>,
              "OBSERVED: std::span over a synchronizable element is refused");

void touch(std::span<SyncCell> cells) {
    for (auto &cell : cells)
        cell.value = 1;
}

int main() {
    std::vector<SyncCell> data(4);
    threadsafe::asynchronous_task_launcher launcher;
    // launch_scoped_task exists precisely so a borrow may cross into a joined
    // thread: it asks for sendable only, never for lifetime_aware.
    launcher.launch_scoped_task(&touch, std::span<SyncCell>{data});
}
