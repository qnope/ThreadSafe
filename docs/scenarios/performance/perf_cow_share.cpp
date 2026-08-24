#include "perf_bench.h"

#include <threadsafe/threadsafe.h>

#include <atomic>
#include <barrier>
#include <cstdio>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {

constexpr int repetitions = 21;

struct configuration {
    std::vector<int> entries;
};

configuration make_configuration(std::size_t entry_count) {
    configuration result;
    result.entries.resize(entry_count, 7);
    return result;
}

long long read_configuration(const configuration& value) {
    long long sum = 0;
    for (int entry : value.entries)
        sum += entry;
    return sum;
}

std::atomic<long long> sink{0};

template <class PerThreadBody>
double run_threads(int thread_count, long long ops_per_thread,
                   PerThreadBody body) {
    std::barrier start_barrier{thread_count + 1};
    std::vector<std::jthread> workers;
    workers.reserve(std::size_t(thread_count));
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
         / double(ops_per_thread * thread_count);
}

template <class PerThreadBody>
double median_of(int thread_count, long long ops, PerThreadBody body) {
    std::vector<double> samples;
    run_threads(thread_count, ops, body);
    for (int repetition = 0; repetition < 5; ++repetition)
        samples.push_back(run_threads(thread_count, ops, body));
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

}

int main() {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    std::printf("A. cost of HANDING THE VALUE TO A READER (the thing COW is "
                "for)\n\n");
    for (std::size_t entry_count : {std::size_t(4), std::size_t(64),
                                    std::size_t(1024),
                                    std::size_t(16384)}) {
        const long long operations = 2'000'000;
        char label[128];
        {
            threadsafe::copy_on_write<configuration> source{
                make_configuration(entry_count)};
            std::snprintf(label, sizeof(label),
                          "%6zu entries: copy_on_write handle copy",
                          entry_count);
            bench::report(label, bench::measure(repetitions, operations, [&] {
                              for (long long i = 0; i < operations; ++i) {
                                  threadsafe::copy_on_write<configuration>
                                      handed_over = source;
                                  bench::do_not_optimize(handed_over);
                              }
                          }));
        }
        {
            configuration source = make_configuration(entry_count);
            std::snprintf(label, sizeof(label),
                          "%6zu entries: plain by-value deep copy",
                          entry_count);
            bench::report(label, bench::measure(repetitions, operations, [&] {
                              for (long long i = 0; i < operations; ++i) {
                                  configuration handed_over = source;
                                  bench::do_not_optimize(handed_over);
                              }
                          }));
        }
        std::printf("\n");
    }

    std::printf("B. N threads READING the same value, 1024-entry payload, "
                "200000 reads/thread\n\n");
    std::printf("%5s | %16s %16s %16s %16s\n", "thr", "cow (own handle)",
                "cow (shared obj)", "sync_value(shm)", "mutex-guarded");
    constexpr long long read_operations = 200'000;
    for (int thread_count : {1, 2, 4, 6, 8, 12}) {
        threadsafe::copy_on_write<configuration> shared_cow{
            make_configuration(1024)};
        const double own_handle_ns =
            median_of(thread_count, read_operations, [&](int) {
                threadsafe::copy_on_write<configuration> local = shared_cow;
                long long sum = 0;
                for (long long i = 0; i < read_operations; ++i) {
                    sum += read_configuration(*local);
                    bench::clobber();
                }
                sink.fetch_add(sum, std::memory_order_relaxed);
            });
        const double shared_object_ns =
            median_of(thread_count, read_operations, [&](int) {
                long long sum = 0;
                for (long long i = 0; i < read_operations; ++i) {
                    sum += read_configuration(*shared_cow);
                    bench::clobber();
                }
                sink.fetch_add(sum, std::memory_order_relaxed);
            });

        threadsafe::synchronized_value<configuration> synchronized{
            make_configuration(1024)};
        const double synchronized_ns =
            median_of(thread_count, read_operations, [&](int) {
                long long sum = 0;
                for (long long i = 0; i < read_operations; ++i) {
                    auto guard = synchronized.lock_shared();
                    sum += read_configuration(*guard);
                }
                sink.fetch_add(sum, std::memory_order_relaxed);
            });

        configuration plain = make_configuration(1024);
        std::mutex plain_mutex;
        const double mutex_ns =
            median_of(thread_count, read_operations, [&](int) {
                long long sum = 0;
                for (long long i = 0; i < read_operations; ++i) {
                    std::lock_guard<std::mutex> guard{plain_mutex};
                    sum += read_configuration(plain);
                }
                sink.fetch_add(sum, std::memory_order_relaxed);
            });

        std::printf("%5d | %16.1f %16.1f %16.1f %16.1f\n", thread_count,
                    own_handle_ns, shared_object_ns, synchronized_ns, mutex_ns);
    }
    bench::do_not_optimize(sink.load());
}
