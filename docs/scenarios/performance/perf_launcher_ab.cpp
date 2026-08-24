#include "perf_bench.h"

#include <threadsafe/threadsafe.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

constexpr int rounds = 61;
constexpr long long tasks_per_round = 300;

std::atomic<long long> sink{0};

template <std::size_t Bytes>
struct heavy_payload {
    std::array<char, Bytes> storage{};
};

template <std::size_t Bytes>
void consume(heavy_payload<Bytes> payload) {
    long long total = 0;
    for (std::size_t index = 0; index < Bytes; index += 512)
        total += payload.storage[index];
    sink.fetch_add(total, std::memory_order_relaxed);
}

template <class Body>
double timed(Body body) {
    const auto start = bench::clock_type::now();
    body();
    const auto stop = bench::clock_type::now();
    return std::chrono::duration<double, std::nano>(stop - start).count()
         / double(tasks_per_round);
}

template <std::size_t Bytes>
void compare(const char* size_label) {
    heavy_payload<Bytes> argument{};
    std::vector<double> launcher_samples, hand_samples;

    for (int round = 0; round < rounds; ++round) {
        // alternate which one goes first, so ordering cannot bias the pair
        const bool launcher_first = (round % 2) == 0;
        auto run_launcher = [&] {
            return timed([&] {
                threadsafe::asynchronous_task_launcher launcher;
                for (long long i = 0; i < tasks_per_round; ++i)
                    launcher.launch_task(consume<Bytes>, argument);
            });
        };
        auto run_hand = [&] {
            return timed([&] {
                std::vector<std::jthread> threads;
                for (long long i = 0; i < tasks_per_round; ++i)
                    threads.emplace_back(consume<Bytes>, argument);
            });
        };
        if (launcher_first) {
            launcher_samples.push_back(run_launcher());
            hand_samples.push_back(run_hand());
        } else {
            hand_samples.push_back(run_hand());
            launcher_samples.push_back(run_launcher());
        }
    }
    std::sort(launcher_samples.begin(), launcher_samples.end());
    std::sort(hand_samples.begin(), hand_samples.end());
    const double launcher_median = launcher_samples[rounds / 2];
    const double hand_median = hand_samples[rounds / 2];
    std::printf("%8s | launch_task %9.0f ns | emplace_back %9.0f ns | "
                "delta %+7.0f ns (%+5.1f%%)\n",
                size_label, launcher_median, hand_median,
                launcher_median - hand_median,
                100.0 * (launcher_median - hand_median) / hand_median);
}

}

int main() {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::printf("A/B alternated, %d rounds of %lld spawns each, median per "
                "spawn\n\n",
                rounds, tasks_per_round);
    compare<8>("     8 B");
    compare<512>("   512 B");
    compare<4096>("  4096 B");
    compare<65536>(" 65536 B");
    compare<262144>("256 KiB");
    bench::do_not_optimize(sink.load());
}
