#include <threadsafe/threadsafe.h>
#include <vector>
namespace { struct SyncType { int payload; }; }
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    std::vector<SyncType*> borrows;
    launcher.launch_task([](std::vector<SyncType*>) {}, borrows);
}
