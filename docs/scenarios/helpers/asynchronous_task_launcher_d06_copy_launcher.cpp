#include <threadsafe/threadsafe.h>
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    threadsafe::asynchronous_task_launcher duplicate = launcher;   // <-- ?
    (void)duplicate;
}
