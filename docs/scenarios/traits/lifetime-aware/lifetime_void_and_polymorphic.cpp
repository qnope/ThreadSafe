#include <threadsafe/threadsafe.h>

#include <cstdlib>
#include <memory>
#include <print>
#include <string_view>

struct SyncBase {
    virtual ~SyncBase() = default;
    virtual void run() const = 0;
};
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncBase);

struct BorrowingDerived : SyncBase {
    std::string_view borrowed;
    void run() const override {}
};

using FreeDeleter = void (*)(void *);

template <class T>
void report(const char *label) {
    std::println("{:44} lifetime_aware={:5} sendable={:5}", label,
                 threadsafe::is_lifetime_aware_v<T>,
                 threadsafe::is_sendable_v<T>);
}

int main() {
    report<void>("void");
    report<std::unique_ptr<void, FreeDeleter>>("unique_ptr<void, void(*)(void*)>");
    report<std::shared_ptr<void>>("shared_ptr<void>");
    report<BorrowingDerived>("BorrowingDerived (holds a string_view)");
    report<std::unique_ptr<SyncBase>>("unique_ptr<SyncBase>");
    report<std::shared_ptr<SyncBase>>("shared_ptr<SyncBase>");
    std::println("launchable_task<void(*)(shared_ptr<SyncBase>), shared_ptr<SyncBase>> = {}",
                 threadsafe::launchable_task<void (*)(std::shared_ptr<SyncBase>),
                                             std::shared_ptr<SyncBase>>);
    return 0;
}
