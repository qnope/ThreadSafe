#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>

struct CounterView {
    std::atomic<int> *target;
};

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(is_sendable_v<CounterView>,
              "a pointer to a synchronizable type is sendable");
static_assert(!is_lifetime_aware_v<CounterView>,
              "CounterView borrows: it must never reach launch_task by value");
static_assert(!is_lifetime_aware_v<std::unique_ptr<CounterView>>,
              "unique_ptr forwards the question to its pointee");

static_assert(is_lifetime_aware_v<std::shared_ptr<CounterView>>,
              "shared_ptr answers true whatever it points at");

using SynchronizedView = threadsafe::synchronized_value<CounterView>;
static_assert(is_synchronizable_v<SynchronizedView>);
static_assert(!is_lifetime_aware_v<SynchronizedView>,
              "synchronized_value forwards the question to T");

using SharedSynchronizedView = std::shared_ptr<SynchronizedView>;
static_assert(is_sendable_v<SharedSynchronizedView>);
static_assert(is_lifetime_aware_v<SharedSynchronizedView>,
              "and the shared_ptr wrapper erases the answer synchronized_value gave");

static_assert(!threadsafe::launchable_task<void (*)(CounterView), CounterView>,
              "the bare borrow is correctly refused");
static_assert(threadsafe::launchable_task<void (*)(SharedSynchronizedView),
                                          SharedSynchronizedView>,
              "the same borrow, wrapped, is accepted");

static_assert(is_lifetime_aware_v<std::shared_ptr<std::atomic<CounterView>>>
                  && is_sendable_v<std::shared_ptr<std::atomic<CounterView>>>,
              "same hole with no library helper involved");

int main() { return 0; }
