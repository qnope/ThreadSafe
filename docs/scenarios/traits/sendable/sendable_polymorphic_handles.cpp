#include <threadsafe/threadsafe.h>

#include <memory>
#include <print>
#include <vector>

// A classic mutex-protected service interface. The author vouches that *using a
// Logger* from several threads is safe, which is exactly what the opt-in door
// is for.
struct Logger {
    virtual ~Logger() = default;
    virtual void write(int) = 0;
};

// The concrete type the handle actually points at. Nothing about it is
// synchronized, and it borrows.
struct FileLogger : Logger {
    int* borrowed_counter;
    void write(int v) override { *borrowed_counter += v; }
};

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Logger);

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

int main() {
    std::println("is_sendable<FileLogger>              = {}", is_sendable_v<FileLogger>);
    std::println("is_sendable<Logger>                  = {}", is_sendable_v<Logger>);
    std::println("is_sendable<Logger*>                 = {}", is_sendable_v<Logger*>);
    std::println("is_sendable<Logger&>                 = {}", is_sendable_v<Logger&>);
    std::println("is_sendable<shared_ptr<Logger>>      = {}", is_sendable_v<std::shared_ptr<Logger>>);
    std::println("is_sendable<weak_ptr<Logger>>        = {}", is_sendable_v<std::weak_ptr<Logger>>);
    std::println("is_sendable<unique_ptr<Logger>>      = {}", is_sendable_v<std::unique_ptr<Logger>>);
    std::println("is_sendable<vector<shared_ptr<Logger>>> = {}",
                 is_sendable_v<std::vector<std::shared_ptr<Logger>>>);
    std::println("is_sendable<reference_wrapper<Logger>>  = {}",
                 is_sendable_v<std::reference_wrapper<Logger>>);
}
