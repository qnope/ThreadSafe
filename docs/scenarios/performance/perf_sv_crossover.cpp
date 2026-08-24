#include "perf_bench.h"

#include <threadsafe/threadsafe.h>

#include <atomic>
#include <barrier>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {

constexpr int repetitions = 5;
constexpr long long operations_per_thread = 20'000;

std::atomic<long long> sink{0};

long long spin_work(const volatile long long* source, int iterations) {
    long long sum = 0;
    for (int index = 0; index < iterations; ++index)
        sum += source[index & 7];
    return sum;
}

template <class PerThreadBody>
double run_threads(int thread_count, PerThreadBody body) {
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

struct guarded_by_mutex {
    std::mutex mutex_;
    volatile long long data_[8]{1, 2, 3, 4, 5, 6, 7, 8};
};
struct guarded_by_shared_mutex {
    std::shared_mutex mutex_;
    volatile long long data_[8]{1, 2, 3, 4, 5, 6, 7, 8};
};

}

int main() {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    std::printf("A. 100%% READS. How long must the critical section be before "
                "shared_mutex beats mutex?\n");
    std::printf("   (ns per operation, median of %d runs, %lld ops/thread)\n\n",
                repetitions, operations_per_thread);
    std::printf("%14s |", "crit.section");
    for (int thread_count : {2, 4, 6, 8, 12})
        std::printf(" %5dthr mx/shm ", thread_count);
    std::printf("\n");

    for (int work_iterations : {0, 8, 32, 128, 512, 2048, 8192}) {
        std::printf("%11d it |", work_iterations);
        for (int thread_count : {2, 4, 6, 8, 12}) {
            guarded_by_mutex mutex_subject;
            guarded_by_shared_mutex shared_subject;
            const double mutex_ns = median_of(thread_count, [&](int) {
                long long local = 0;
                for (long long i = 0; i < operations_per_thread; ++i) {
                    std::lock_guard<std::mutex> guard{mutex_subject.mutex_};
                    local += spin_work(mutex_subject.data_, work_iterations);
                }
                sink.fetch_add(local, std::memory_order_relaxed);
            });
            const double shared_ns = median_of(thread_count, [&](int) {
                long long local = 0;
                for (long long i = 0; i < operations_per_thread; ++i) {
                    std::shared_lock<std::shared_mutex> guard{
                        shared_subject.mutex_};
                    local += spin_work(shared_subject.data_, work_iterations);
                }
                sink.fetch_add(local, std::memory_order_relaxed);
            });
            std::printf(" %7.0f/%-7.0f", mutex_ns, shared_ns);
        }
        std::printf("\n");
    }

    std::printf("\nB. 100%% WRITES (exclusive lock only). mutex vs "
                "shared_mutex's unique_lock.\n\n");
    std::printf("%6s | %14s %14s %10s\n", "thr", "std::mutex(ns)",
                "shared_mx(ns)", "penalty");
    for (int thread_count : {1, 2, 4, 6, 8, 12}) {
        guarded_by_mutex mutex_subject;
        guarded_by_shared_mutex shared_subject;
        const double mutex_ns = median_of(thread_count, [&](int) {
            for (long long i = 0; i < operations_per_thread; ++i) {
                std::lock_guard<std::mutex> guard{mutex_subject.mutex_};
                mutex_subject.data_[0] = i;
            }
        });
        const double shared_ns = median_of(thread_count, [&](int) {
            for (long long i = 0; i < operations_per_thread; ++i) {
                std::unique_lock<std::shared_mutex> guard{shared_subject.mutex_};
                shared_subject.data_[0] = i;
            }
        });
        std::printf("%6d | %14.1f %14.1f %9.1fx\n", thread_count, mutex_ns,
                    shared_ns, shared_ns / mutex_ns);
    }
    bench::do_not_optimize(sink.load());
}
