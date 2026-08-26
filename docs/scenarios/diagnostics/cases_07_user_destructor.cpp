#include <threadsafe/threadsafe.h>

struct HasDestructor {
    int value;
    ~HasDestructor() {}
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](HasDestructor value) { (void)value; }, HasDestructor{});
}
