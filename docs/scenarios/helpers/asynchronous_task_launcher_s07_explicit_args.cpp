#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <memory>

namespace {
struct SyncBox {
    std::shared_ptr<std::atomic<int>> cell = std::make_shared<std::atomic<int>>(7);
};
void read_box(SyncBox box) { std::printf("task sees cell=%p\n", (void*)box.cell.get()); }
}

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncBox);

// launch_task is immune: lifetime_aware<T&> is hard-wired false.
static_assert(!threadsafe::launchable_task<void(*)(SyncBox), SyncBox&>,
              "launch_task<..., T&> is rejected");
// launch_scoped_task is not.
static_assert(threadsafe::launchable_scoped_task<void(*)(SyncBox), SyncBox&>,
              "POLARITY: launch_scoped_task<..., T&> is accepted");

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    SyncBox box;
    std::printf("before: box.cell = %p\n", (void*)box.cell.get());
    launcher.launch_scoped_task<void(*)(SyncBox), SyncBox&>(&read_box, box);
    std::printf("after:  box.cell = %p  <-- moved out of the caller's lvalue\n",
                (void*)box.cell.get());
    return box.cell == nullptr ? 0 : 1;
}
