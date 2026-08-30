#include <threadsafe/threadsafe.h>

struct Borrowing {
    int *borrowed;
};

struct Middle {
    Borrowing inner;
};

struct Outer {
    Middle middle;
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](Outer) {}, Outer{});
}
