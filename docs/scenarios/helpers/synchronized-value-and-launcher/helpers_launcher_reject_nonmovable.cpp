#include <threadsafe/threadsafe.h>
#include <atomic>
namespace { struct SyncCounter { std::atomic<int> counter{0}; void operator()() const {} }; }
template <> struct threadsafe::is_synchronizable<SyncCounter> : std::true_type {};
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    SyncCounter counter;
    launcher.launch_task(counter);
}
