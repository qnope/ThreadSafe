#include <threadsafe/threadsafe.h>

struct HandRolledCopy {
    int value = 0;
    HandRolledCopy() = default;
    HandRolledCopy(const HandRolledCopy &other) : value(other.value) {}
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](HandRolledCopy) {}, HandRolledCopy{});
}
