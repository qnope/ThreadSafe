#include <threadsafe/threadsafe.h>

#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

namespace {
struct SyncType { int payload; };
}
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);

template <class T>
void dump(const char* name) {
    std::cout << name
              << "  send=" << is_sendable_v<T>
              << "  sync=" << is_synchronizable_v<T>
              << "  const_sync=" << is_synchronizable_v<const T>
              << "  life=" << is_lifetime_aware_v<T>
              << "  launchable=" << threadsafe::launchable_task<decltype([](T) {}), T>
              << '\n';
}

using ref_to_atomic = std::reference_wrapper<std::atomic<int>>;
using sync_ref = threadsafe::synchronized_value<ref_to_atomic>;

int main() {
    dump<std::string_view>("string_view                     ");
    dump<std::span<int>>("span<int>                       ");
    dump<std::atomic<int>>("atomic<int>                     ");
    dump<ref_to_atomic>("reference_wrapper<atomic<int>>  ");
    dump<std::shared_ptr<std::atomic<int>>>("shared_ptr<atomic<int>>         ");
    dump<sync_ref>("synchronized_value<ref<atomic>> ");
    dump<std::shared_ptr<sync_ref>>("shared_ptr<sv<ref<atomic>>>     ");
    dump<std::atomic<SyncType*>>("atomic<SyncType*>               ");
    dump<std::shared_ptr<std::atomic<SyncType*>>>("shared_ptr<atomic<SyncType*>>   ");
    dump<std::shared_ptr<std::string_view>>("shared_ptr<string_view>         ");
}
