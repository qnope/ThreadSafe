// is_synchronizable<const shared_ptr<T>> asks only about the POINTEE. The
// deleter and the allocator live type-erased in the control block and are never
// inspected. Copying a const shared_ptr across threads therefore hands the
// deleter's captured state to whichever thread drops the last reference.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(is_synchronizable_v<const std::shared_ptr<std::atomic<int>>>,
              "the library calls a const shared_ptr to a synchronizable pointee "
              "readable from several threads at once");
static_assert(is_sendable_v<std::shared_ptr<std::atomic<int>>>);

int main() {
    int deleter_visible_counter = 0;

    {
        std::shared_ptr<std::atomic<int>> owner{
            new std::atomic<int>{0},
            [&deleter_visible_counter](std::atomic<int> *raw) {
                deleter_visible_counter += 1;   // plain int, no synchronization
                delete raw;
            }};

        const std::shared_ptr<std::atomic<int>> &read_only_handle = owner;

        std::thread worker{[copy = read_only_handle,
                            &deleter_visible_counter]() mutable {
            copy->fetch_add(1, std::memory_order_relaxed);
            copy.reset();                      // may run the erased deleter here
            deleter_visible_counter += 0;
        }};

        owner.reset();                          // ... or here, on the main thread
        deleter_visible_counter += 0;
        worker.join();
    }

    std::printf("deleter ran %d time(s)\n", deleter_visible_counter);
    return 0;
}
