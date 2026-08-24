#include <threadsafe/threadsafe.h>
struct DeviceContext { int handle = 0; };
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(DeviceContext);
template <> struct threadsafe::is_sendable<DeviceContext> : std::false_type {};
static_assert(!threadsafe::is_sendable_v<DeviceContext>);
static_assert(threadsafe::is_synchronizable_v<DeviceContext>);
static_assert(threadsafe::is_sendable_v<DeviceContext&>,
              "but a reference to it is still sendable, which is right");
int main() {}
