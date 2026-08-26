#include <threadsafe/threadsafe.h>
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(42);                  // not callable at all
}
