#include <threadsafe/threadsafe.h>
#include <functional>
namespace { struct Blessed { void operator()() const {} }; }
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Blessed);
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    Blessed blessed;
    launcher.launch_task(std::ref(blessed));   // sendable, not lifetime aware
}
