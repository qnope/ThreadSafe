#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <numeric>
#include <ranges>

using IotaRange = std::ranges::iota_view<int, int>;

template <>
struct threadsafe::is_lifetime_aware<IotaRange> : std::true_type {};

static_assert(threadsafe::launchable_task<void (*)(IotaRange), IotaRange>);

void sum_range(IotaRange range) {
    std::printf("sum = %d\n", std::accumulate(range.begin(), range.end(), 0));
}

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(&sum_range, std::views::iota(0, 10));
    return 0;
}
