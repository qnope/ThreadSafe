// Follow the diagnostic's advice: "specialize is_sendable to state the intent"
// for a capturing lambda. A lambda that captures only a shared_ptr to a
// synchronized_value is perfectly safe; the library still refuses it.
#include <threadsafe/threadsafe.h>
#include <memory>

using counter = threadsafe::synchronized_value<int>;

// Both polarities: captureless is fine, one safe capture is not.
static_assert(threadsafe::is_sendable_v<decltype([] {})>);
static_assert(!threadsafe::is_sendable_v<
              decltype([total = std::shared_ptr<counter>{}] { (void)total; })>);
static_assert(threadsafe::is_sendable_v<std::shared_ptr<counter>>
                  && threadsafe::is_lifetime_aware_v<std::shared_ptr<counter>>,
              "the only thing captured answers both traits");

int main() {
    auto total = counter::make(0);
    threadsafe::asynchronous_task_launcher launcher;
    // Cannot specialize is_sendable here: a specialization must be at namespace
    // scope, and the closure type has no name outside this function.
    launcher.launch_task([total] {
        auto guard = total->lock();
        *guard += 1;
    });
}
