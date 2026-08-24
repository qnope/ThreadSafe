#include <threadsafe/threadsafe.h>

#include <atomic>
#include <barrier>
#include <condition_variable>
#include <latch>
#include <mutex>
#include <semaphore>
#include <shared_mutex>
#include <print>

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

template <class T>
void report(const char *name) {
    std::println("{:38} sync={:5} const-sync={:5} send={:5} life={:5}", name,
                 is_synchronizable_v<T>, is_synchronizable_v<const T>,
                 is_sendable_v<T>, is_lifetime_aware_v<T>);
}

int main() {
    report<std::atomic<int>>("std::atomic<int>");
    report<std::atomic_flag>("std::atomic_flag");
    report<std::atomic_ref<int>>("std::atomic_ref<int>");
    report<std::mutex>("std::mutex");
    report<std::recursive_mutex>("std::recursive_mutex");
    report<std::shared_mutex>("std::shared_mutex");
    report<std::condition_variable>("std::condition_variable");
    report<std::condition_variable_any>("std::condition_variable_any");
    report<std::once_flag>("std::once_flag");
    report<std::counting_semaphore<4>>("std::counting_semaphore<4>");
    report<std::binary_semaphore>("std::binary_semaphore");
    report<std::latch>("std::latch");
    report<std::barrier<>>("std::barrier<>");
}
