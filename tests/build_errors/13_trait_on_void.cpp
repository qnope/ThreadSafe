#include <threadsafe/threadsafe.h>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](void *pointer) {}, static_cast<void *>(nullptr));
}
