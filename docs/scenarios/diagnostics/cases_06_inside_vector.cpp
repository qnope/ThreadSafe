#include <threadsafe/threadsafe.h>
#include <vector>

struct Leaf { int *pointer; };
struct Holder { std::vector<Leaf> leaves; };

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](Holder holder) { (void)holder; }, Holder{});
}
