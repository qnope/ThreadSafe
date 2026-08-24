#include <threadsafe/threadsafe.h>

#include <atomic>
#include <memory>

struct CounterView {
    std::atomic<int> *target;
};

consteval bool explain() {
    threadsafe::assert_lifetime_aware<std::shared_ptr<CounterView>>();
    return true;
}

static_assert(explain());

int main() { return 0; }
