#include <threadsafe/threadsafe.h>

#include <memory>

class Implementation;

struct Pimpl {
    std::unique_ptr<Implementation> implementation;
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](Pimpl) {}, Pimpl{});
}
