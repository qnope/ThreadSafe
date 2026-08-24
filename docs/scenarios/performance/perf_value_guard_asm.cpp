#include <threadsafe/threadsafe.h>

#include <mutex>
#include <shared_mutex>

struct payload {
    int counters[8];
};

using library_value = threadsafe::synchronized_value<payload>;

int via_guard(library_value& guarded) {
    auto held = guarded.lock();
    return held->counters[0];
}

int via_hand(std::shared_mutex& mutex, payload& value) {
    std::lock_guard<std::shared_mutex> held{mutex};
    return value.counters[0];
}

int via_hand_unique_lock(std::shared_mutex& mutex, payload& value) {
    std::unique_lock<std::shared_mutex> held{mutex};
    return value.counters[0];
}
