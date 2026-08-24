#include "perf_bench.h"

#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

constexpr int repetitions = 31;

struct configuration {
    std::vector<int> entries;
    std::string label;
};

configuration make_configuration(std::size_t entry_count) {
    configuration result;
    result.entries.resize(entry_count);
    for (std::size_t index = 0; index < entry_count; ++index)
        result.entries[index] = int(index);
    result.label = "configuration";
    return result;
}

long long read_configuration(const configuration& value) {
    long long sum = 0;
    for (int entry : value.entries)
        sum += entry;
    return sum + static_cast<long long>(value.label.size());
}

}

int main() {
    for (std::size_t entry_count : {std::size_t(4), std::size_t(64),
                                    std::size_t(1024)}) {
        std::printf("\n=== payload: vector<int> of %zu entries (%zu bytes of "
                    "heap) ===\n",
                    entry_count, entry_count * sizeof(int));

        const long long read_operations = 2'000'000;
        {
            threadsafe::copy_on_write<configuration> subject{
                make_configuration(entry_count)};
            bench::report("READ  copy_on_write operator*",
                          bench::measure(repetitions, read_operations, [&] {
                              long long sum = 0;
                              for (long long i = 0; i < read_operations; ++i) {
                                  sum += read_configuration(*subject);
                                  bench::clobber();
                              }
                              bench::do_not_optimize(sum);
                          }));
        }
        {
            std::shared_ptr<const configuration> subject =
                std::make_shared<const configuration>(
                    make_configuration(entry_count));
            bench::report("READ  shared_ptr<const T> operator*",
                          bench::measure(repetitions, read_operations, [&] {
                              long long sum = 0;
                              for (long long i = 0; i < read_operations; ++i) {
                                  sum += read_configuration(*subject);
                                  bench::clobber();
                              }
                              bench::do_not_optimize(sum);
                          }));
        }
        {
            configuration subject = make_configuration(entry_count);
            std::mutex mutex;
            bench::report("READ  mutex-guarded value",
                          bench::measure(repetitions, read_operations, [&] {
                              long long sum = 0;
                              for (long long i = 0; i < read_operations; ++i) {
                                  std::lock_guard<std::mutex> guard{mutex};
                                  sum += read_configuration(subject);
                              }
                              bench::do_not_optimize(sum);
                          }));
        }

        const long long write_operations = 200'000;
        {
            threadsafe::copy_on_write<configuration> subject{
                make_configuration(entry_count)};
            bench::report("WRITE copy_on_write as_mutable() UNSHARED",
                          bench::measure(repetitions, write_operations, [&] {
                              for (long long i = 0; i < write_operations; ++i) {
                                  subject.as_mutable().entries[0] = int(i);
                                  bench::clobber();
                              }
                          }));
        }
        {
            threadsafe::copy_on_write<configuration> subject{
                make_configuration(entry_count)};
            threadsafe::copy_on_write<configuration> observer = subject;
            bench::do_not_optimize(observer);
            bench::report("WRITE copy_on_write as_mutable() SHARED(1 reader)",
                          bench::measure(repetitions, write_operations, [&] {
                              for (long long i = 0; i < write_operations; ++i) {
                                  threadsafe::copy_on_write<configuration>
                                      keep_shared = subject;
                                  subject.as_mutable().entries[0] = int(i);
                                  bench::do_not_optimize(keep_shared);
                              }
                          }));
        }
        {
            configuration subject = make_configuration(entry_count);
            bench::report("WRITE plain by-value copy then mutate",
                          bench::measure(repetitions, write_operations, [&] {
                              for (long long i = 0; i < write_operations; ++i) {
                                  configuration copy = subject;
                                  copy.entries[0] = int(i);
                                  bench::do_not_optimize(copy);
                              }
                          }));
        }
        {
            configuration subject = make_configuration(entry_count);
            std::mutex mutex;
            bench::report("WRITE mutex-guarded in-place mutate",
                          bench::measure(repetitions, write_operations, [&] {
                              for (long long i = 0; i < write_operations; ++i) {
                                  std::lock_guard<std::mutex> guard{mutex};
                                  subject.entries[0] = int(i);
                              }
                          }));
        }
    }

    std::printf("\n=== isolated cost of the as_mutable() bookkeeping "
                "(no payload work) ===\n");
    const long long probe_operations = 20'000'000;
    {
        threadsafe::copy_on_write<long long> subject{0};
        bench::report("as_mutable() on unshared cow<long long>",
                      bench::measure(repetitions, probe_operations, [&] {
                          for (long long i = 0; i < probe_operations; ++i) {
                              subject.as_mutable() = i;
                              bench::clobber();
                          }
                      }));
    }
    {
        std::shared_ptr<long long> subject = std::make_shared<long long>(0);
        bench::report("shared_ptr<long long> write, no use_count check",
                      bench::measure(repetitions, probe_operations, [&] {
                          for (long long i = 0; i < probe_operations; ++i) {
                              *subject = i;
                              bench::clobber();
                          }
                      }));
    }
    {
        std::shared_ptr<long long> subject = std::make_shared<long long>(0);
        bench::report("shared_ptr<long long> use_count() load only",
                      bench::measure(repetitions, probe_operations, [&] {
                          long long sum = 0;
                          for (long long i = 0; i < probe_operations; ++i) {
                              sum += subject.use_count();
                              bench::clobber();
                          }
                          bench::do_not_optimize(sum);
                      }));
    }
    {
        long long value = 0;
        bench::report("std::atomic_thread_fence(acquire) alone",
                      bench::measure(repetitions, probe_operations, [&] {
                          for (long long i = 0; i < probe_operations; ++i) {
                              std::atomic_thread_fence(
                                  std::memory_order_acquire);
                              value = i;
                              bench::clobber();
                          }
                      }));
        bench::do_not_optimize(value);
    }
    {
        long long value = 0;
        bench::report("plain long long write (baseline)",
                      bench::measure(repetitions, probe_operations, [&] {
                          for (long long i = 0; i < probe_operations; ++i) {
                              value = i;
                              bench::clobber();
                          }
                      }));
        bench::do_not_optimize(value);
    }
}
