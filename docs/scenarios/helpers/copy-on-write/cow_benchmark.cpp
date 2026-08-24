#include <threadsafe/threadsafe.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using threadsafe::copy_on_write;
using clock_type = std::chrono::steady_clock;

namespace {
struct Config {
    std::vector<int> table = std::vector<int>(256, 1);
    std::string name = "configuration-object-with-a-heap-allocated-name";
};

constexpr int read_iterations = 20'000'000;
constexpr int write_iterations = 2'000'000;
constexpr int share_iterations = 5'000'000;

template <class Body>
double time_ms(Body&& body) {
    const auto started = clock_type::now();
    body();
    const auto finished = clock_type::now();
    return std::chrono::duration<double, std::milli>(finished - started).count();
}

volatile long long sink = 0;
}

int main() {
    // ---------------- read-heavy: one holder, no sharing ------------------
    {
        copy_on_write<Config> cow_config{Config{}};
        std::shared_ptr<const Config> shared_config = std::make_shared<const Config>();
        Config plain_config;

        const double cow_ms = time_ms([&] {
            long long accumulated = 0;
            for (int iteration = 0; iteration != read_iterations; ++iteration)
                accumulated += cow_config->table[iteration & 255];
            sink = accumulated;
        });
        const double shared_ms = time_ms([&] {
            long long accumulated = 0;
            for (int iteration = 0; iteration != read_iterations; ++iteration)
                accumulated += shared_config->table[iteration & 255];
            sink = accumulated;
        });
        const double plain_ms = time_ms([&] {
            long long accumulated = 0;
            for (int iteration = 0; iteration != read_iterations; ++iteration)
                accumulated += plain_config.table[iteration & 255];
            sink = accumulated;
        });
        std::printf("READ    %d iters | cow %8.1f ms | shared_ptr<const T> %8.1f ms | plain %8.1f ms\n",
                    read_iterations, cow_ms, shared_ms, plain_ms);
    }

    // ---------------- share-heavy: copy the handle ------------------------
    {
        copy_on_write<Config> cow_config{Config{}};
        std::shared_ptr<const Config> shared_config = std::make_shared<const Config>();
        Config plain_config;

        const double cow_ms = time_ms([&] {
            for (int iteration = 0; iteration != share_iterations; ++iteration) {
                copy_on_write<Config> handle = cow_config;
                sink += handle->table[0];
            }
        });
        const double shared_ms = time_ms([&] {
            for (int iteration = 0; iteration != share_iterations; ++iteration) {
                std::shared_ptr<const Config> handle = shared_config;
                sink += handle->table[0];
            }
        });
        const double plain_ms = time_ms([&] {
            for (int iteration = 0; iteration != share_iterations / 50; ++iteration) {
                Config handle = plain_config;
                sink += handle.table[0];
            }
        });
        std::printf("SHARE   %d iters | cow %8.1f ms | shared_ptr<const T> %8.1f ms | plain-copy %8.1f ms (only %d iters!)\n",
                    share_iterations, cow_ms, shared_ms, plain_ms,
                    share_iterations / 50);
    }

    // ---------------- write-heavy, UNSHARED (the COW fast path) -----------
    {
        copy_on_write<Config> cow_config{Config{}};
        Config plain_config;

        const double cow_ms = time_ms([&] {
            for (int iteration = 0; iteration != write_iterations; ++iteration)
                cow_config.as_mutable().table[iteration & 255] = iteration;
        });
        const double plain_ms = time_ms([&] {
            for (int iteration = 0; iteration != write_iterations; ++iteration)
                plain_config.table[iteration & 255] = iteration;
        });
        std::printf("WRITE-U %d iters | cow %8.1f ms | plain %8.1f ms\n",
                    write_iterations, cow_ms, plain_ms);
    }

    // ---------------- write-heavy, SHARED (every write detaches) ----------
    {
        copy_on_write<Config> cow_config{Config{}};
        copy_on_write<Config> pinned_reader = cow_config;

        const double cow_ms = time_ms([&] {
            for (int iteration = 0; iteration != write_iterations / 20; ++iteration) {
                copy_on_write<Config> observer = cow_config;
                cow_config.as_mutable().table[iteration & 255] = iteration;
            }
        });
        std::printf("WRITE-S %d iters | cow %8.1f ms (a full Config copy per write)\n",
                    write_iterations / 20, cow_ms);
        sink += pinned_reader->table[0];
    }
    return 0;
}
