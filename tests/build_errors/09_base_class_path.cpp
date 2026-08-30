#include <threadsafe/threadsafe.h>

struct Borrowing {
    int *borrowed;
};

struct DerivedFromBorrowing : Borrowing {
    int own_value = 0;
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](DerivedFromBorrowing) {}, DerivedFromBorrowing{});
}
