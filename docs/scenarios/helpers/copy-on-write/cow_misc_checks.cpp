#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

namespace { struct SyncCache { mutable std::atomic<int> hits; }; }
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncCache);

using threadsafe::copy_on_write;
static_assert(!threadsafe::is_synchronizable_v<copy_on_write<SyncCache>>,
              "a vouched-synchronizable T does NOT leak into the handle");
static_assert(threadsafe::is_sendable_v<copy_on_write<SyncCache>>);
static_assert(!threadsafe::is_lifetime_aware_v<copy_on_write<int*>>);
static_assert(threadsafe::is_lifetime_aware_v<copy_on_write<std::string>>);
static_assert(!threadsafe::is_lifetime_aware_v<copy_on_write<std::string_view>>);

int main() {
    copy_on_write<std::string> source{"text"};
    copy_on_write<std::string> destination = std::move(source);
    std::printf("moved-from operator-> == %p\n",
                static_cast<const void*>(source.operator->()));
    std::printf("destination = %s\n", destination->c_str());
    return 0;
}
