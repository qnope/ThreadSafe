#include <threadsafe/threadsafe.h>

#include <print>

// A thread-affine handle: it may be *used* from any thread (every operation
// takes its own lock), but it must be released on the thread that created it —
// the Rust MutexGuard shape, which is Sync but deliberately not Send.
struct DeviceContext {
    int handle = 0;
};

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(DeviceContext);

int main() {
    std::println("is_synchronizable<DeviceContext> = {}",
                 threadsafe::is_synchronizable_v<DeviceContext>);
    std::println("is_sendable<DeviceContext>       = {}  <- never asked for",
                 threadsafe::is_sendable_v<DeviceContext>);
    std::println("launchable_task<void(*)(DeviceContext), DeviceContext> = {}",
                 threadsafe::launchable_task<void (*)(DeviceContext), DeviceContext>);
}
