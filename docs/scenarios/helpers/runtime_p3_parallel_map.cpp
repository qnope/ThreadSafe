// Program 3 -- PARALLEL MAP over 10 million doubles.
// The question is only "how do I hand thread i its slice", because spans,
// pointers and shared_ptr<const vector<double>> are all rejected.
//
// build: g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread \
//            p3_parallel_map.cpp -o p3 && ./p3
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <span>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t element_count = 10'000'000;
constexpr int worker_count = 8;

double map_element(double value) { return value * value + 1.0; }

// ---------------------------------------------------------------------------
// A. shared_ptr<synchronized_value<vector<double>>>: the only zero-copy slice
//    the traits accept. Every worker takes ONE shared lock and holds it for the
//    whole slice -- a reader lock used as a permission slip, not as exclusion.
// ---------------------------------------------------------------------------
using guarded_data = threadsafe::synchronized_value<std::vector<double>>;

struct guarded_slice_task {
    std::shared_ptr<guarded_data> data;
    std::shared_ptr<threadsafe::synchronized_value<double>> total;
    std::size_t first;
    std::size_t last;

    void operator()() const {
        double partial = 0.0;
        {
            const auto guard = data->lock_shared();
            for (std::size_t index = first; index < last; ++index)
                partial += map_element((*guard)[index]);
        }
        auto total_guard = total->lock();
        *total_guard += partial;
    }
};

// ---------------------------------------------------------------------------
// B. the workaround a user reaches for once spans are refused: copy the slice.
// ---------------------------------------------------------------------------
struct copied_slice_task {
    std::vector<double> slice;
    std::shared_ptr<threadsafe::synchronized_value<double>> total;

    void operator()() const {
        double partial = 0.0;
        for (double value : slice)
            partial += map_element(value);
        auto total_guard = total->lock();
        *total_guard += partial;
    }
};

struct timing { double milliseconds; double total; };

timing run_guarded(const std::vector<double>& source) {
    auto data = guarded_data::make(source);
    auto total = threadsafe::synchronized_value<double>::make(0.0);
    const auto started_at = std::chrono::steady_clock::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        const std::size_t chunk = element_count / worker_count;
        for (int worker = 0; worker < worker_count; ++worker)
            launcher.launch_task(guarded_slice_task{
                data, total, worker * chunk,
                worker + 1 == worker_count ? element_count
                                           : (worker + 1) * chunk});
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    const auto guard = total->lock_shared();
    return {std::chrono::duration<double, std::milli>(elapsed).count(), *guard};
}

timing run_copied(const std::vector<double>& source) {
    auto total = threadsafe::synchronized_value<double>::make(0.0);
    const auto started_at = std::chrono::steady_clock::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        const std::size_t chunk = element_count / worker_count;
        for (int worker = 0; worker < worker_count; ++worker) {
            const std::size_t first = worker * chunk;
            const std::size_t last = worker + 1 == worker_count
                                         ? element_count
                                         : (worker + 1) * chunk;
            launcher.launch_task(copied_slice_task{
                std::vector<double>(source.begin() + first,
                                    source.begin() + last),
                total});
        }
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    const auto guard = total->lock_shared();
    return {std::chrono::duration<double, std::milli>(elapsed).count(), *guard};
}

timing run_bare_jthread(const std::vector<double>& source) {
    std::atomic<double> total{0.0};
    const auto started_at = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> threads;
        const std::size_t chunk = element_count / worker_count;
        for (int worker = 0; worker < worker_count; ++worker) {
            const std::size_t first = worker * chunk;
            const std::size_t last = worker + 1 == worker_count
                                         ? element_count
                                         : (worker + 1) * chunk;
            threads.emplace_back([&total, slice = std::span<const double>{
                                              source.data() + first,
                                              last - first}] {
                double partial = 0.0;
                for (double value : slice)
                    partial += map_element(value);
                total += partial;
            });
        }
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return {std::chrono::duration<double, std::milli>(elapsed).count(),
            total.load()};
}

}

// The three slice shapes a user tries first, and what the library says.
static_assert(!threadsafe::is_sendable_v<std::span<const double>>);
static_assert(!threadsafe::is_lifetime_aware_v<std::span<const double>>);
static_assert(!threadsafe::is_sendable_v<const double*>);
static_assert(!threadsafe::is_lifetime_aware_v<const double*>);
static_assert(!threadsafe::is_sendable_v<std::shared_ptr<const std::vector<double>>>,
              "the canonical immutable-share handle is rejected");
static_assert(threadsafe::is_sendable_v<std::shared_ptr<guarded_data>>,
              "only the mutex-wrapped form gets through");
static_assert(std::is_same_v<guarded_data::mutex, std::shared_mutex>);

int main() {
    std::vector<double> source(element_count);
    for (std::size_t index = 0; index < element_count; ++index)
        source[index] = double(index % 1024) * 0.5;

    const auto guarded = run_guarded(source);
    const auto copied = run_copied(source);
    const auto bare = run_bare_jthread(source);

    std::printf("%zu elements, %d workers\n\n", element_count, worker_count);
    std::printf("A. shared_ptr<synchronized_value<vector>>, one shared_lock "
                "per worker : %7.1f ms  sum=%.6e\n",
                guarded.milliseconds, guarded.total);
    std::printf("B. per-worker std::vector<double> copy of the slice         "
                "           : %7.1f ms  sum=%.6e\n",
                copied.milliseconds, copied.total);
    std::printf("C. bare std::jthread + std::span (rejected by the library)  "
                "           : %7.1f ms  sum=%.6e\n",
                bare.milliseconds, bare.total);
}
