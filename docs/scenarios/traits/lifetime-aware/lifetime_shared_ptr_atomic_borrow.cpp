#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>

struct CounterView {
    std::atomic<int> *target;
};

static_assert(threadsafe::is_sendable_v<std::shared_ptr<std::atomic<CounterView>>>);
static_assert(threadsafe::is_lifetime_aware_v<std::shared_ptr<std::atomic<CounterView>>>);
static_assert(threadsafe::launchable_task<
                  void (*)(std::shared_ptr<std::atomic<CounterView>>),
                  std::shared_ptr<std::atomic<CounterView>>>,
              "no library helper, no unsafe macro, still accepted");

int main() { return 0; }
