#include <threadsafe/threadsafe.h>
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    int local = 0;
    launcher.launch_task([local] { (void)local; });
}
