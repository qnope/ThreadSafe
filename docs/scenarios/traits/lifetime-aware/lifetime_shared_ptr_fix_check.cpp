#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>
#include <span>

struct CounterView {
    std::atomic<int> *target;
};
using SynchronizedView = threadsafe::synchronized_value<CounterView>;

static_assert(!threadsafe::is_lifetime_aware_v<std::shared_ptr<CounterView>>);
static_assert(!threadsafe::is_lifetime_aware_v<std::weak_ptr<CounterView>>);
static_assert(!threadsafe::is_lifetime_aware_v<std::shared_ptr<SynchronizedView>>);
static_assert(!threadsafe::is_lifetime_aware_v<std::shared_ptr<std::atomic<CounterView>>>);
static_assert(!threadsafe::launchable_task<void (*)(std::shared_ptr<SynchronizedView>),
                                           std::shared_ptr<SynchronizedView>>);

static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<int>>);
static_assert(threadsafe::is_lifetime_aware_v<std::weak_ptr<int>>);
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<std::atomic<int>>>);
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<void>>);
static_assert(!threadsafe::is_lifetime_aware_v<std::shared_ptr<std::span<int>>>);

int main() { return 0; }
