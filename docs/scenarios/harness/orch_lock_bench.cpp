// Independent check of the report's claim that synchronized_value costs the same
// as the hand-written equivalent.
#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <algorithm>
#include <vector>

namespace {
constexpr int kIterations = 20'000'000;

template <class Body>
double nanos_per_op(Body body) {
    body();  // warm up
    std::vector<double> samples;
    for (int run = 0; run < 5; ++run) {
        auto start = std::chrono::steady_clock::now();
        body();
        auto stop = std::chrono::steady_clock::now();
        samples.push_back(
            std::chrono::duration<double, std::nano>(stop - start).count() / kIterations);
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];   // median
}
}

int main() {
    // (a) the library
    threadsafe::synchronized_value<int> guarded{0};
    const double library = nanos_per_op([&] {
        for (int i = 0; i < kIterations; ++i) {
            auto guard = guarded.lock();
            ++*guard;
        }
    });

    // (b) the same thing written by hand
    std::shared_mutex mutex;
    int value = 0;
    const double by_hand = nanos_per_op([&] {
        for (int i = 0; i < kIterations; ++i) {
            std::unique_lock lock(mutex);
            ++value;
        }
    });

    std::printf("synchronized_value<int>::lock()  %.2f ns/op\n", library);
    std::printf("hand-written shared_mutex        %.2f ns/op\n", by_hand);
    std::printf("difference                       %+.2f ns  (%.1f%%)\n",
                library - by_hand, 100.0 * (library - by_hand) / by_hand);
    auto final_guard = guarded.lock();
    std::printf("checksum %d %d\n", *final_guard, value);
}
