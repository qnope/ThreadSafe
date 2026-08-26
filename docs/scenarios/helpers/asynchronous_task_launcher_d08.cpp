#include <threadsafe/threadsafe.h>
namespace { struct Blessed { int value = 0; }; void use(Blessed*) {} }
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Blessed);
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    Blessed blessed;
    launcher.launch_task(&use, &blessed);      // arg sendable, not lifetime aware
}
