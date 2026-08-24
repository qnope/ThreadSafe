// The most common beginner mistake: hand a std::shared_ptr<int> to a thread.
// The trait says no. Ask the library WHY.
#include <threadsafe/threadsafe.h>

#include <memory>

static_assert(!threadsafe::is_sendable_v<std::shared_ptr<int>>,
              "a shared_ptr<int> lets two threads write the same int");

consteval bool why() {
    threadsafe::assert_sendable<std::shared_ptr<int>>();
    return true;
}

static_assert(why());
