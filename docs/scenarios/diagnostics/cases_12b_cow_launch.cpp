#include <threadsafe/threadsafe.h>

struct MutableCounter {
    int value;
    mutable int cached;
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(
        [](threadsafe::copy_on_write<MutableCounter> shared) { (void)shared; },
        threadsafe::copy_on_write<MutableCounter>{MutableCounter{}});
}
