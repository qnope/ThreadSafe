#include <threadsafe/threadsafe.h>

struct B { int *pointer; };
struct A { B b; };
struct Root { A a; };

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](Root root) { (void)root; }, Root{});
}
