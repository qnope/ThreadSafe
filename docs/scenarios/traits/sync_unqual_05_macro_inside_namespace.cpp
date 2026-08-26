#include <threadsafe/threadsafe.h>

namespace application {

struct RegistryHandle {};

// The macro hard-codes `threadsafe::`; used inside any namespace that does not
// enclose ::threadsafe it is ill-formed.
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(RegistryHandle);

}

static_assert(threadsafe::is_synchronizable_v<application::RegistryHandle>);

int main() {}
