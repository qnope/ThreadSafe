#include <threadsafe/threadsafe.h>

#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {
struct SyncType { int payload; };
struct HoldsPointer { SyncType* borrowed; };
struct HoldsRefWrapper { std::reference_wrapper<SyncType> borrowed; };
struct HoldsSharedString { std::shared_ptr<std::string> owned; };
struct HoldsUniqueString { std::unique_ptr<std::string> owned; };
using DeepBorrow =
    std::tuple<std::vector<std::pair<int, std::reference_wrapper<SyncType>>>>;
}
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);

template <class T>
void report(const char* name) {
    std::cout << (threadsafe::launchable_task<decltype([](T) {}), T>
                      ? "ACCEPTED  " : "rejected  ")
              << name << '\n';
}

int main() {
    report<HoldsPointer>("HoldsPointer                                       ");
    report<HoldsRefWrapper>("HoldsRefWrapper                                    ");
    report<HoldsSharedString>("struct{ shared_ptr<string> }                       ");
    report<HoldsUniqueString>("struct{ unique_ptr<string> }                       ");
    report<DeepBorrow>("tuple<vector<pair<int, reference_wrapper<Sync>>>>  ");
    report<std::vector<std::vector<SyncType*>>>("vector<vector<SyncType*>>                          ");
    report<std::shared_ptr<threadsafe::synchronized_value<HoldsPointer>>>(
        "shared_ptr<synchronized_value<HoldsPointer>>       ");
    report<std::shared_ptr<threadsafe::synchronized_value<HoldsRefWrapper>>>(
        "shared_ptr<synchronized_value<HoldsRefWrapper>>    ");
    report<std::shared_ptr<std::atomic<SyncType*>>>(
        "shared_ptr<atomic<SyncType*>>                      ");
    report<std::unique_ptr<threadsafe::synchronized_value<HoldsPointer>>>(
        "unique_ptr<synchronized_value<HoldsPointer>>       ");
    report<std::shared_ptr<threadsafe::synchronized_value<std::string>>>(
        "shared_ptr<synchronized_value<string>>  (intended) ");
}
