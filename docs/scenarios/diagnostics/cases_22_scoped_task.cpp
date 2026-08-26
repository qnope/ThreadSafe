#include <threadsafe/threadsafe.h>

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    int counter = 0;
    launcher.launch_scoped_task([](int *p) { (void)p; }, &counter);
}
