#include <threadsafe/threadsafe.h>

#include <memory>

struct Base {
    virtual ~Base() = default;
    virtual void run() const {}
};

template <>
struct threadsafe::is_unsafe_sendable<Base> {
    static consteval threadsafe::TraitAnswer diagnose() { return {}; }
};

template <>
struct threadsafe::is_unsafe_lifetime_aware<Base> {
    static consteval threadsafe::TraitAnswer diagnose() { return {}; }
};

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task([](std::unique_ptr<Base> base) { base->run(); },
                         std::make_unique<Base>());
}
