#include <threadsafe/threadsafe.h>

#include <atomic>
#include <functional>

// std::atomic_flag is the one type [atomics.flag] guarantees is always
// lock-free and race-free. std::atomic<bool>, sitting right next to it, is
// accepted by the library. std::atomic_flag is not.
static_assert(threadsafe::is_synchronizable_v<std::atomic<bool>>);
static_assert(!threadsafe::is_synchronizable_v<std::atomic_flag>);

// The consequences, all of which are false rejections:
static_assert(!threadsafe::is_sendable_v<std::atomic_flag&>,
              "a reference to an atomic_flag cannot be shared");
static_assert(!threadsafe::is_sendable_v<std::atomic_flag*>,
              "a pointer to an atomic_flag cannot be shared");
static_assert(!threadsafe::is_sendable_v<std::reference_wrapper<std::atomic_flag>>,
              "std::ref of an atomic_flag cannot be sent");
static_assert(!threadsafe::is_sendable_v<std::shared_ptr<std::atomic_flag>>,
              "a shared_ptr to an atomic_flag cannot be sent");

struct SpinLatch {
    std::atomic_flag taken;
};
static_assert(!threadsafe::is_synchronizable_v<SpinLatch*>
                  || true);
static_assert(!threadsafe::is_sendable_v<SpinLatch*>,
              "a spin latch built on atomic_flag cannot be shared either");

// And the launcher refuses the canonical stop-flag pattern:
static_assert(!threadsafe::launchable_task<
                  decltype([](std::atomic_flag&) {}),
                  std::reference_wrapper<std::atomic_flag>>);

int main() {}
