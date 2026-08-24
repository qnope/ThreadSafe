// The rest of the suite is compile-time only. These three helpers have runtime
// behaviour that no static_assert can reach — std::make_shared and std::mutex
// are not usable in a constant expression — so this translation unit has a
// main() and is meant to be run.
#include <threadsafe/threadsafe.h>

#include <cassert>
#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using threadsafe::copy_on_write;
using threadsafe::synchronized_value;

namespace {

void copy_on_write_detaches_only_when_shared() {
    copy_on_write<std::vector<int>> original(std::size_t{3}, 7);

    auto shared_handle = original;
    shared_handle.as_mutable().push_back(42);
    assert(original->size() == 3
           && "a write through a shared handle must not touch the other one");
    assert(shared_handle->size() == 4);

    // A unique handle must hand back the same object every time: detaching
    // here would be a silent copy on the hot path.
    copy_on_write<std::vector<int>> unique_handle(std::size_t{2}, 1);
    const int *first_address = unique_handle.as_mutable().data();
    const int *second_address = unique_handle.as_mutable().data();
    assert(first_address == second_address
           && "a unique handle must not copy");

    // And once the other handle is gone, the survivor is unique again.
    {
        auto temporary_handle = unique_handle;
        (void)temporary_handle.as_mutable();
    }
    const int *after_release = unique_handle.as_mutable().data();
    assert(after_release == first_address
           && "dropping the other handle restores uniqueness");
}

void synchronized_value_serializes_writers() {
    constexpr int writer_count = 8;
    constexpr int increments_per_writer = 10000;

    synchronized_value<int> counter{0};
    std::vector<std::jthread> writers;
    for (int writer = 0; writer < writer_count; ++writer)
        writers.emplace_back([&counter] {
            for (int i = 0; i < increments_per_writer; ++i) {
                auto guard = counter.lock();
                ++*guard;
            }
        });
    writers.clear();

    auto final_guard = counter.lock();
    assert(*final_guard == writer_count * increments_per_writer
           && "every increment must survive");
}

void shared_readers_observe_a_consistent_value() {
    // T is const-synchronizable, so the wrapper picks a shared_mutex and the
    // readers below really do run concurrently.
    static_assert(std::is_same_v<synchronized_value<std::string>::mutex,
                                 std::shared_mutex>);

    synchronized_value<std::string> text{std::string(1000, 'a')};
    std::vector<std::jthread> readers;
    for (int reader = 0; reader < 8; ++reader)
        readers.emplace_back([&text] {
            for (int i = 0; i < 1000; ++i) {
                auto guard = text.lock_shared();
                assert(guard->size() == 1000);
                assert(guard->front() == guard->back());
            }
        });
    readers.clear();
}

void launcher_joins_every_task_at_destruction() {
    // A raw pointer keeps nothing alive, so the launcher refuses it: the
    // shared_ptr is the checked way to hand the counter to the tasks.
    auto completed = std::make_shared<std::atomic<int>>(0);
    {
        threadsafe::asynchronous_task_launcher launcher;
        for (int task = 0; task < 16; ++task)
            launcher.launch_task(
                [](std::shared_ptr<std::atomic<int>> done) { ++*done; },
                completed);
    }
    assert(completed->load() == 16
           && "the launcher's destructor must join every task");
}

}

int main() {
    copy_on_write_detaches_only_when_shared();
    synchronized_value_serializes_writers();
    shared_readers_observe_a_consistent_value();
    launcher_joins_every_task_at_destruction();
    std::puts("runtime behaviour: OK");
}
