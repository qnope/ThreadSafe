#include <threadsafe/threadsafe.h>

#include <atomic>
#include <barrier>
#include <condition_variable>
#include <functional>
#include <latch>
#include <mutex>
#include <print>
#include <semaphore>
#include <shared_mutex>

using namespace threadsafe;

int main() {
    std::println("is_sendable<std::mutex>                          = {}", is_sendable_v<std::mutex>);
    std::println("is_synchronizable<std::mutex>                    = {}", is_synchronizable_v<std::mutex>);
    std::println("is_synchronizable<const std::mutex>              = {}", is_synchronizable_v<const std::mutex>);
    std::println("is_sendable<std::mutex&>                         = {}", is_sendable_v<std::mutex&>);
    std::println("is_sendable<std::latch&>                         = {}", is_sendable_v<std::latch&>);
    std::println("is_sendable<std::shared_mutex&>                  = {}", is_sendable_v<std::shared_mutex&>);
    std::println("is_sendable<std::counting_semaphore<>&>          = {}", is_sendable_v<std::counting_semaphore<>&>);
    std::println("is_sendable<std::condition_variable&>            = {}", is_sendable_v<std::condition_variable&>);
    std::println("is_sendable<std::barrier<>&>                     = {}", is_sendable_v<std::barrier<>&>);
    std::println("is_sendable<std::once_flag&>                     = {}", is_sendable_v<std::once_flag&>);
    std::println("is_sendable<std::atomic<int>&>                   = {}", is_sendable_v<std::atomic<int>&>);
    std::println("is_sendable<std::reference_wrapper<std::latch>>  = {}", is_sendable_v<std::reference_wrapper<std::latch>>);
    std::println("is_sendable<std::mutex*>                         = {}", is_sendable_v<std::mutex*>);
    std::println("is_lifetime_aware<std::mutex>                    = {}", is_lifetime_aware_v<std::mutex>);
    std::println("is_lifetime_aware<std::latch>                    = {}", is_lifetime_aware_v<std::latch>);
    std::println("is_sendable<synchronized_value<int>&>            = {}", is_sendable_v<synchronized_value<int>&>);
    std::println("is_sendable<std::shared_ptr<synchronized_value<int>>> = {}", is_sendable_v<std::shared_ptr<synchronized_value<int>>>);
}
