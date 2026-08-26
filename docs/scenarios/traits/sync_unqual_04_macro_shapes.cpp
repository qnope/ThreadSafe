#include <threadsafe/threadsafe.h>

#include <map>
#include <string>

namespace application {
struct RegistryHandle {};

template <class Payload>
struct Slot {
    Payload payload;
};
}

// (1) namespace-qualified type, macro at global scope
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(application::RegistryHandle);

// (2) a class template specialization
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(application::Slot<int>);

// (3) a template-id with a comma in the argument list
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(std::map<int, int>);

// (4) a const type
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(const std::string);

using threadsafe::is_synchronizable_v;

static_assert(is_synchronizable_v<application::RegistryHandle>);
static_assert(is_synchronizable_v<application::Slot<int>>);
static_assert(!is_synchronizable_v<application::Slot<double>>);
static_assert(is_synchronizable_v<std::map<int, int>>);
static_assert(is_synchronizable_v<const std::string>);

int main() {}
