// What does the library TELL the user who wrote shared_ptr<const Configuration>?
#include <threadsafe/threadsafe.h>

#include <map>
#include <memory>
#include <string>

using Configuration = std::map<std::string, std::string>;

consteval bool explain() {
    threadsafe::assert_sendable<std::shared_ptr<const Configuration>>();
    return true;
}

static_assert(explain());
