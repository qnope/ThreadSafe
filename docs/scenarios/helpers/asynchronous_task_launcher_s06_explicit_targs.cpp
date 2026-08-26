#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <memory>

namespace {
// A handle its author vouches for as SHAREABLE -- it synchronizes itself, so
// several threads may touch the same object -- but explicitly forbids being
// SENT: duplicating the handle onto another thread is what breaks it.
struct SharedCounterHandle {
    std::shared_ptr<std::atomic<int>> counter =
        std::make_shared<std::atomic<int>>(0);
    void operator()() const { counter->fetch_add(1); }
};
}

template <> struct threadsafe::is_synchronizable<SharedCounterHandle> : std::true_type {};
template <> struct threadsafe::is_sendable<SharedCounterHandle>       : std::false_type {};

static_assert(!threadsafe::is_sendable_v<SharedCounterHandle>,
              "its author says: never copy me onto another thread");
static_assert(threadsafe::is_sendable_v<SharedCounterHandle&>,
              "sendable<T&> is is_synchronizable<T>, which is true");

// The deduced call is correctly rejected: F decays to SharedCounterHandle.
static_assert(!threadsafe::launchable_scoped_task<SharedCounterHandle>);

// Naming F explicitly as a reference satisfies the constraint instead.
static_assert(threadsafe::launchable_scoped_task<SharedCounterHandle&>,
              "POLARITY: launchable_scoped_task<F&> is satisfied");

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    SharedCounterHandle handle;
    std::printf("before: handle.counter = %p\n", (void *)handle.counter.get());

    // F is a reference now, so `F f` is a reference parameter bound to `handle`,
    // and the body's std::move(f) MOVES FROM THE CALLER'S OBJECT.
    launcher.launch_scoped_task<SharedCounterHandle&>(handle);

    std::printf("after:  handle.counter = %p  <-- the launcher gutted it\n",
                (void *)handle.counter.get());
    return handle.counter == nullptr ? 0 : 1;
}
