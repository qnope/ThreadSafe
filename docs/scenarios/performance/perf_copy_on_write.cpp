#include <threadsafe/threadsafe.h>

#include "perf_bench.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

// A payload big enough that a copy is not free, small enough to stay realistic.
struct configuration {
    std::vector<int> table;
    std::string name;

    configuration() : table(256, 1), name("configuration") {}
};

std::uint64_t sink = 0;

std::uint64_t read_configuration(const configuration& value) {
    std::uint64_t total = value.name.size();
    for (int entry : value.table)
        total += std::uint64_t(entry);
    return total;
}

constexpr std::uint64_t read_iterations = 2000000;
constexpr std::uint64_t write_iterations = 200000;

}

int main() {
    std::printf("sizeof(copy_on_write<configuration>) = %zu\n",
                sizeof(threadsafe::copy_on_write<configuration>));
    std::printf("sizeof(std::shared_ptr<const configuration>) = %zu\n",
                sizeof(std::shared_ptr<const configuration>));
    std::printf("sizeof(configuration) = %zu, table entries = 256\n\n",
                sizeof(configuration));

    // ---------------- READ PATH ----------------
    {
        threadsafe::copy_on_write<configuration> cow{};
        std::shared_ptr<const configuration> shared =
            std::make_shared<const configuration>();
        configuration plain{};
        std::mutex mutex;

        bench::report("read: copy_on_write operator->",
            bench::measure(2, 9, read_iterations, [&] {
                std::uint64_t local = 0;
                for (std::uint64_t index = 0; index < read_iterations; ++index)
                    local += read_configuration(*cow);
                sink += local;
            }));
        bench::report("read: shared_ptr<const T> operator*",
            bench::measure(2, 9, read_iterations, [&] {
                std::uint64_t local = 0;
                for (std::uint64_t index = 0; index < read_iterations; ++index)
                    local += read_configuration(*shared);
                sink += local;
            }));
        bench::report("read: plain value",
            bench::measure(2, 9, read_iterations, [&] {
                std::uint64_t local = 0;
                for (std::uint64_t index = 0; index < read_iterations; ++index)
                    local += read_configuration(plain);
                sink += local;
            }));
        bench::report("read: mutex-guarded value",
            bench::measure(2, 9, read_iterations, [&] {
                std::uint64_t local = 0;
                for (std::uint64_t index = 0; index < read_iterations; ++index) {
                    std::lock_guard<std::mutex> held{mutex};
                    local += read_configuration(plain);
                }
                sink += local;
            }));
        std::printf("\n");
    }

    // ---------------- WRITE PATH, UNSHARED (use_count()==1) ----------------
    {
        threadsafe::copy_on_write<configuration> cow{};
        configuration plain{};
        std::mutex mutex;
        std::shared_ptr<configuration> shared = std::make_shared<configuration>();

        bench::report("write UNSHARED: cow.as_mutable()",
            bench::measure(2, 9, write_iterations, [&] {
                for (std::uint64_t index = 0; index < write_iterations; ++index)
                    cow.as_mutable().table[index & 255] += 1;
            }));
        bench::report("write UNSHARED: plain value",
            bench::measure(2, 9, write_iterations, [&] {
                for (std::uint64_t index = 0; index < write_iterations; ++index)
                    plain.table[index & 255] += 1;
            }));
        bench::report("write UNSHARED: mutex-guarded value",
            bench::measure(2, 9, write_iterations, [&] {
                for (std::uint64_t index = 0; index < write_iterations; ++index) {
                    std::lock_guard<std::mutex> held{mutex};
                    plain.table[index & 255] += 1;
                }
            }));
        bench::report("write UNSHARED: shared_ptr deref",
            bench::measure(2, 9, write_iterations, [&] {
                for (std::uint64_t index = 0; index < write_iterations; ++index)
                    shared->table[index & 255] += 1;
            }));
        sink += std::uint64_t(cow->table[0]) + std::uint64_t(plain.table[0])
              + std::uint64_t(shared->table[0]);
        std::printf("\n");
    }

    // ---------------- WRITE PATH, SHARED (use_count()>1 -> real copy) ------
    {
        threadsafe::copy_on_write<configuration> cow{};
        auto keeper = cow;                 // forces every as_mutable() to copy
        std::shared_ptr<const configuration> shared =
            std::make_shared<const configuration>();
        configuration plain{};

        bench::report("write SHARED: cow.as_mutable() (copies each time)",
            bench::measure(2, 9, write_iterations / 10, [&] {
                for (std::uint64_t index = 0; index < write_iterations / 10; ++index) {
                    auto copy = cow;       // re-share so the next write copies
                    copy.as_mutable().table[index & 255] += 1;
                    sink += std::uint64_t(copy->table[0]);
                }
            }));
        bench::report("write SHARED: full by-value copy + mutate",
            bench::measure(2, 9, write_iterations / 10, [&] {
                for (std::uint64_t index = 0; index < write_iterations / 10; ++index) {
                    configuration copy = plain;
                    copy.table[index & 255] += 1;
                    sink += std::uint64_t(copy.table[0]);
                }
            }));
        bench::report("write SHARED: shared_ptr<const T> copy-on-publish",
            bench::measure(2, 9, write_iterations / 10, [&] {
                for (std::uint64_t index = 0; index < write_iterations / 10; ++index) {
                    auto copy = std::make_shared<configuration>(*shared);
                    copy->table[index & 255] += 1;
                    sink += std::uint64_t(copy->table[0]);
                }
            }));
        std::printf("\n");
    }

    // ---------------- COST OF THE use_count() CHECK ALONE -----------------
    {
        threadsafe::copy_on_write<int> cow{0};
        std::shared_ptr<int> shared = std::make_shared<int>(0);
        int plain = 0;
        volatile int* plain_pointer = &plain;

        bench::report("as_mutable() on unshared int (use_count + fence)",
            bench::measure(2, 9, read_iterations, [&] {
                for (std::uint64_t index = 0; index < read_iterations; ++index)
                    cow.as_mutable() += 1;
            }));
        bench::report("shared_ptr<int> deref + increment",
            bench::measure(2, 9, read_iterations, [&] {
                for (std::uint64_t index = 0; index < read_iterations; ++index)
                    *shared += 1;
            }));
        bench::report("bare volatile int increment",
            bench::measure(2, 9, read_iterations, [&] {
                for (std::uint64_t index = 0; index < read_iterations; ++index)
                    *plain_pointer = *plain_pointer + 1;
            }));
        bench::report("use_count()!=1 test only (no fence, no write)",
            bench::measure(2, 9, read_iterations, [&] {
                std::uint64_t local = 0;
                for (std::uint64_t index = 0; index < read_iterations; ++index)
                    local += (shared.use_count() != 1) ? 1u : 0u;
                sink += local;
            }));
        bench::report("atomic_thread_fence(acquire) only",
            bench::measure(2, 9, read_iterations, [&] {
                for (std::uint64_t index = 0; index < read_iterations; ++index)
                    std::atomic_thread_fence(std::memory_order_acquire);
            }));
        sink += std::uint64_t(*cow) + std::uint64_t(*shared) + std::uint64_t(plain);
    }

    std::printf("\nsink=%llu\n", (unsigned long long)sink);
}
