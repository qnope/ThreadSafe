#include <threadsafe/threadsafe.h>

#include <functional>
#include <latch>
#include <mutex>
#include <print>

using threadsafe::is_sendable_v;

int main() {
    std::println("is_sendable<std::mutex&>                        = {}", is_sendable_v<std::mutex&>);
    std::println("is_sendable<std::latch&>                        = {}", is_sendable_v<std::latch&>);
    std::println("is_sendable<std::shared_mutex&>                 = {}", is_sendable_v<std::shared_mutex&>);
    std::println("is_sendable<std::counting_semaphore<>&>         = {}", is_sendable_v<std::counting_semaphore<>&>);
    std::println("is_sendable<std::condition_variable&>           = {}", is_sendable_v<std::condition_variable&>);
    std::println("is_sendable<std::atomic<int>&>                  = {}", is_sendable_v<std::atomic<int>&>);
    std::println("is_sendable<std::reference_wrapper<std::latch>> = {}",
                 is_sendable_v<std::reference_wrapper<std::latch>>);
}
