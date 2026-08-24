#include "perf_bench.h"

#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <mutex>
#include <shared_mutex>

namespace {

constexpr int repetitions = 41;
constexpr long long operations = 2'000'000;

struct hand_rolled_mutex_counter {
    mutable std::mutex mutex_;
    long long value_ = 0;
};

struct hand_rolled_shared_mutex_counter {
    mutable std::shared_mutex mutex_;
    long long value_ = 0;
};

}

int main() {
    std::printf("uncontended (single thread), %lld ops per repetition, %d "
                "repetitions\n\n",
                operations, repetitions);

    {
        threadsafe::synchronized_value<long long> subject{0};
        bench::report("synchronized_value<long long>::lock()  [write]",
                      bench::measure(repetitions, operations, [&] {
                          for (long long i = 0; i < operations; ++i) {
                              auto guard = subject.lock();
                              *guard += 1;
                          }
                      }));
    }
    {
        threadsafe::synchronized_value<long long> subject{0};
        bench::report("synchronized_value<long long>::lock_shared() [read]",
                      bench::measure(repetitions, operations, [&] {
                          long long sum = 0;
                          for (long long i = 0; i < operations; ++i) {
                              auto guard = subject.lock_shared();
                              sum += *guard;
                          }
                          bench::do_not_optimize(sum);
                      }));
    }
    {
        hand_rolled_mutex_counter subject;
        bench::report("hand std::mutex + lock_guard           [write]",
                      bench::measure(repetitions, operations, [&] {
                          for (long long i = 0; i < operations; ++i) {
                              std::lock_guard<std::mutex> guard{subject.mutex_};
                              subject.value_ += 1;
                          }
                      }));
    }
    {
        hand_rolled_mutex_counter subject;
        bench::report("hand std::mutex + lock_guard           [read]",
                      bench::measure(repetitions, operations, [&] {
                          long long sum = 0;
                          for (long long i = 0; i < operations; ++i) {
                              std::lock_guard<std::mutex> guard{subject.mutex_};
                              sum += subject.value_;
                          }
                          bench::do_not_optimize(sum);
                      }));
    }
    {
        hand_rolled_shared_mutex_counter subject;
        bench::report("hand std::shared_mutex + unique_lock   [write]",
                      bench::measure(repetitions, operations, [&] {
                          for (long long i = 0; i < operations; ++i) {
                              std::unique_lock<std::shared_mutex> guard{
                                  subject.mutex_};
                              subject.value_ += 1;
                          }
                      }));
    }
    {
        hand_rolled_shared_mutex_counter subject;
        bench::report("hand std::shared_mutex + shared_lock   [read]",
                      bench::measure(repetitions, operations, [&] {
                          long long sum = 0;
                          for (long long i = 0; i < operations; ++i) {
                              std::shared_lock<std::shared_mutex> guard{
                                  subject.mutex_};
                              sum += subject.value_;
                          }
                          bench::do_not_optimize(sum);
                      }));
    }
    {
        long long value = 0;
        bench::report("no lock at all (baseline)",
                      bench::measure(repetitions, operations, [&] {
                          for (long long i = 0; i < operations; ++i) {
                              value += 1;
                              bench::clobber();
                          }
                      }));
        bench::do_not_optimize(value);
    }
}
