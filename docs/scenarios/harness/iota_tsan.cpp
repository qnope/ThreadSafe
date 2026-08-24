#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <numeric>
#include <ranges>

using IotaRange = std::ranges::iota_view<int, int>;

void sum_range(IotaRange range) {
    std::printf("sum = %d\n", std::accumulate(range.begin(), range.end(), 0));
}

int main() {
    threadsafe::asynchronous_task_launcher launcher;
    for (int i = 0; i < 8; ++i)
        launcher.launch_task(&sum_range, std::views::iota(0, 1000));
    return 0;
}
