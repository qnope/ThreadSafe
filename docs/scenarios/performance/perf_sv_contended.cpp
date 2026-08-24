#include "perf_bench.h"

#include <threadsafe/threadsafe.h>

#include <array>
#include <atomic>
#include <barrier>
#include <cstdio>
#include <mutex>
#include <numeric>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {

constexpr int repetitions = 11;
constexpr long long operations_per_thread = 200'000;
constexpr std::size_t payload_size = 16;

using payload = std::array<long long, payload_size>;

long long read_payload(const payload& data) {
    long long sum = 0;
    for (std::size_t index = 0; index < payload_size; ++index)
        sum += data[index];
    return sum;
}

void write_payload(payload& data, long long seed) {
    for (std::size_t index = 0; index < payload_size; ++index)
        data[index] = seed + static_cast<long long>(index);
}

struct hand_mutex_holder {
    mutable std::mutex mutex_;
    payload data_{};
};

struct hand_shared_mutex_holder {
    mutable std::shared_mutex mutex_;
    payload data_{};
};

template <class PerThreadBody>
double run_threads(int thread_count, PerThreadBody body) {
    std::barrier start_barrier{thread_count + 1};
    std::vector<std::jthread> workers;
    workers.reserve(thread_count);
    for (int thread_index = 0; thread_index < thread_count; ++thread_index)
        workers.emplace_back([&, thread_index] {
            start_barrier.arrive_and_wait();
            body(thread_index);
        });

    start_barrier.arrive_and_wait();
    const auto start = bench::clock_type::now();
    workers.clear();
    const auto stop = bench::clock_type::now();
    return std::chrono::duration<double, std::nano>(stop - start).count()
         / double(operations_per_thread * thread_count);
}

template <class PerThreadBody>
double median_of(int thread_count, PerThreadBody body) {
    std::vector<double> samples;
    run_threads(thread_count, body);
    for (int repetition = 0; repetition < repetitions; ++repetition)
        samples.push_back(run_threads(thread_count, body));
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

std::atomic<long long> sink{0};

}

int main() {
    std::printf("contended, %lld ops/thread, %d repetitions, payload = "
                "array<long long,%zu>\n",
                operations_per_thread, repetitions, payload_size);
    std::printf("Apple M3 Pro, 6 performance + 6 efficiency cores\n\n");
    std::printf("%6s %8s | %14s %14s %14s | %s\n", "thr", "write%",
                "sync_value(ns)", "mutex(ns)", "shared_mx(ns)",
                "verdict vs plain std::mutex");
    std::printf("-------------------------------------------------------------"
                "---------------------------------------\n");

    const int thread_counts[] = {1, 2, 4, 6, 8, 12};
    const int write_percents[] = {0, 1, 5, 10, 25, 50, 100};

    for (int thread_count : thread_counts) {
        for (int write_percent : write_percents) {
            threadsafe::synchronized_value<payload> library_subject{};
            hand_mutex_holder mutex_subject;
            hand_shared_mutex_holder shared_mutex_subject;

            const double library_ns = median_of(
                thread_count, [&](int thread_index) {
                    long long local = 0;
                    for (long long i = 0; i < operations_per_thread; ++i) {
                        if (int((i * 7 + thread_index) % 100) < write_percent) {
                            auto guard = library_subject.lock();
                            write_payload(*guard, i);
                        } else {
                            auto guard = library_subject.lock_shared();
                            local += read_payload(*guard);
                        }
                    }
                    sink.fetch_add(local, std::memory_order_relaxed);
                });

            const double mutex_ns = median_of(
                thread_count, [&](int thread_index) {
                    long long local = 0;
                    for (long long i = 0; i < operations_per_thread; ++i) {
                        if (int((i * 7 + thread_index) % 100) < write_percent) {
                            std::lock_guard<std::mutex> guard{
                                mutex_subject.mutex_};
                            write_payload(mutex_subject.data_, i);
                        } else {
                            std::lock_guard<std::mutex> guard{
                                mutex_subject.mutex_};
                            local += read_payload(mutex_subject.data_);
                        }
                    }
                    sink.fetch_add(local, std::memory_order_relaxed);
                });

            const double shared_ns = median_of(
                thread_count, [&](int thread_index) {
                    long long local = 0;
                    for (long long i = 0; i < operations_per_thread; ++i) {
                        if (int((i * 7 + thread_index) % 100) < write_percent) {
                            std::unique_lock<std::shared_mutex> guard{
                                shared_mutex_subject.mutex_};
                            write_payload(shared_mutex_subject.data_, i);
                        } else {
                            std::shared_lock<std::shared_mutex> guard{
                                shared_mutex_subject.mutex_};
                            local += read_payload(shared_mutex_subject.data_);
                        }
                    }
                    sink.fetch_add(local, std::memory_order_relaxed);
                });

            const double ratio = library_ns / mutex_ns;
            std::printf("%6d %7d%% | %14.1f %14.1f %14.1f | %s %.2fx\n",
                        thread_count, write_percent, library_ns, mutex_ns,
                        shared_ns, ratio > 1.0 ? "LOSES" : "wins ", ratio);
        }
        std::printf("\n");
    }
    bench::do_not_optimize(sink.load());
}
