// Read-heavy shared configuration: which way of sharing one immutable map
// does the library accept?
#include <threadsafe/threadsafe.h>

#include <functional>
#include <map>
#include <memory>
#include <print>
#include <string>

using configuration = std::map<std::string, std::string>;

template <class T>
consteval bool sendable() { return threadsafe::is_sendable_v<T>; }

int main() {
    std::println("is_synchronizable_v<const configuration>      = {}",
                 threadsafe::is_synchronizable_v<const configuration>);
    std::println("is_sendable_v<const configuration&>           = {}",
                 sendable<const configuration &>());
    std::println("is_sendable_v<const configuration*>           = {}",
                 sendable<const configuration *>());
    std::println("is_sendable_v<reference_wrapper<const conf>>  = {}",
                 sendable<std::reference_wrapper<const configuration>>());
    std::println("is_sendable_v<shared_ptr<const configuration>>= {}",
                 sendable<std::shared_ptr<const configuration>>());
    std::println("is_sendable_v<shared_ptr<configuration>>      = {}",
                 sendable<std::shared_ptr<configuration>>());
    std::println("is_sendable_v<copy_on_write<configuration>>   = {}",
                 sendable<threadsafe::copy_on_write<configuration>>());
    std::println("is_sendable_v<configuration> (a full copy)    = {}",
                 sendable<configuration>());
}
