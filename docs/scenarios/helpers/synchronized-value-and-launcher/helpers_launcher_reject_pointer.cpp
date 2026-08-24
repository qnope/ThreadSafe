#include <threadsafe/threadsafe.h>
namespace { struct SyncType { int payload; }; }
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    SyncType shared{0};
    launcher.launch_task([](SyncType* borrowed) { (void)borrowed; }, &shared);
}
