#include <threadsafe/threadsafe.h>

struct Borrowing {
    int *borrowed;
};

struct HoldsAnArray {
    Borrowing entries[4];
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](HoldsAnArray) {}, HoldsAnArray{});
}
