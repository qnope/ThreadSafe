#include <threadsafe/threadsafe.h>

struct Base {
    virtual ~Base() = default;
    virtual void run() const {}
};

template <>
struct threadsafe::is_unsafe_synchronizable<Base> : std::true_type {};

int main() {
    Base base;
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](const Base*) {}, &base);
}
