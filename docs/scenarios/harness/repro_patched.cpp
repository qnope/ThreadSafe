#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <memory>
#include <numeric>
#include <ranges>
#include <vector>

using IotaRange = std::ranges::iota_view<int, int>;

static_assert(threadsafe::is_sendable_v<IotaRange>,
              "an iota_view holds two ints; it is sendable");
static_assert(std::move_constructible<IotaRange>);
static_assert(threadsafe::is_lifetime_aware_v<IotaRange>,
              "patched: owns its two ints");
static_assert(threadsafe::launchable_task<void (*)(IotaRange), IotaRange>,
              "patched: launchable");

void sum_range(IotaRange range) {
    std::printf("sum = %d\n", std::accumulate(range.begin(), range.end(), 0));
}

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(&sum_range, std::views::iota(0, 10));
    return 0;
}
