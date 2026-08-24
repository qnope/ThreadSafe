#include <threadsafe/threadsafe.h>
#include <mutex>
#include <shared_mutex>

extern long long global_counter;

void through_value_guard(threadsafe::synchronized_value<long long>& subject) {
    auto guard = subject.lock();
    *guard += 1;
}

void through_lock_guard(std::mutex& mutex, long long& value) {
    std::lock_guard<std::mutex> guard{mutex};
    value += 1;
}

void through_unique_lock_shared_mutex(std::shared_mutex& mutex,
                                      long long& value) {
    std::unique_lock<std::shared_mutex> guard{mutex};
    value += 1;
}

void through_lock_guard_shared_mutex(std::shared_mutex& mutex,
                                     long long& value) {
    std::lock_guard<std::shared_mutex> guard{mutex};
    value += 1;
}
