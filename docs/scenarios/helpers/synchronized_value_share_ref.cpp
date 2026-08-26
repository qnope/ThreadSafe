#include <threadsafe/threadsafe.h>
#include <functional>
int main() {
    threadsafe::synchronized_value<int> shared_counter{0};
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](threadsafe::synchronized_value<int>& counter) {
        const auto guard = counter.lock();
        *guard += 1;
    }, std::ref(shared_counter));
}
