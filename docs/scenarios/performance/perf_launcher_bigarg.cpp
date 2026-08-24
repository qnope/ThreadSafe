#include "perf_bench.h"

#include <threadsafe/threadsafe.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

constexpr int repetitions = 41;
constexpr long long tasks_per_repetition = 300;

std::atomic<long long> sink{0};

template <std::size_t Bytes>
struct heavy_payload {
    std::array<char, Bytes> storage{};
};

template <std::size_t Bytes>
void consume(heavy_payload<Bytes> payload) {
    long long total = 0;
    for (std::size_t index = 0; index < Bytes; index += 4096)
        total += payload.storage[index];
    sink.fetch_add(total, std::memory_order_relaxed);
}

template <std::size_t Bytes>
void compare(const char* size_label) {
    heavy_payload<Bytes> argument{};
    char label[128];

    std::snprintf(label, sizeof(label),
                  "%s arg: launch_task (by value, then move)", size_label);
    bench::report(label, bench::measure(repetitions, tasks_per_repetition, [&] {
                      threadsafe::asynchronous_task_launcher launcher;
                      for (long long i = 0; i < tasks_per_repetition; ++i)
                          launcher.launch_task(consume<Bytes>, argument);
                  }));

    std::snprintf(label, sizeof(label),
                  "%s arg: emplace_back (forwarded, one copy)", size_label);
    bench::report(label, bench::measure(repetitions, tasks_per_repetition, [&] {
                      std::vector<std::jthread> threads;
                      for (long long i = 0; i < tasks_per_repetition; ++i)
                          threads.emplace_back(consume<Bytes>, argument);
                  }));
    std::printf("\n");
}

}

int main() {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::printf("end-to-end spawn cost, %lld tasks per repetition, median of "
                "%d\n\n",
                tasks_per_repetition, repetitions);
    compare<8>("    8 B");
    compare<4096>(" 4096 B");
    compare<65536>("65536 B");
    compare<1048576>("  1 MiB");
    bench::do_not_optimize(sink.load());
}
