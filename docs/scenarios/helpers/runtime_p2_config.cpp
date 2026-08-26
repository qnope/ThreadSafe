// Program 2 -- SHARED READ-MOSTLY CONFIG.
// One writer publishing new configs, eight readers reading continuously.
// Five strategies, timed. Strategy 1 is copy_on_write used the only way the
// traits allow, and it never publishes anything.
//
// build: g++-16 -std=c++26 -freflection -I<threadsafe>/include -O2 -pthread \
//            p2_config.cpp -o p2 && ./p2
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Config {
    std::string name;
    std::vector<int> weights;
    int version;
};

constexpr int reader_count = 8;
constexpr int reads_per_reader = 400'000;
constexpr int publications = 200;

Config make_config(int version) {
    Config config;
    config.name = "config-v" + std::to_string(version);
    config.weights.assign(32, version);
    config.version = version;
    return config;
}

std::uint64_t consume(const Config& config) {
    std::uint64_t total = config.name.size();
    for (int weight : config.weights)
        total += static_cast<std::uint64_t>(weight);
    return total;
}

struct outcome {
    double milliseconds;
    int highest_version_seen;
    std::uint64_t checksum;
};

// ---------------------------------------------------------------------------
// 1. copy_on_write, used the way the traits allow: every thread owns a handle.
//    copy_on_write has no store() and no load(); is_synchronizable<cow<T>> is
//    false, so the writer's handle and the readers' handles are different
//    objects. as_mutable() rebinds the WRITER's handle only.
// ---------------------------------------------------------------------------
struct cow_reader_task {
    threadsafe::copy_on_write<Config> config;
    std::shared_ptr<std::atomic<int>> highest_version_seen;
    std::shared_ptr<std::atomic<std::uint64_t>> checksum;

    void operator()() const {
        std::uint64_t local = 0;
        int local_version = 0;
        for (int read = 0; read < reads_per_reader; ++read) {
            local += consume(*config);
            if (config->version > local_version)
                local_version = config->version;
        }
        *checksum += local;
        int previous = highest_version_seen->load();
        while (previous < local_version
               && !highest_version_seen->compare_exchange_weak(previous,
                                                               local_version))
            ;
    }
};

struct cow_writer_task {
    threadsafe::copy_on_write<Config> config;
    std::shared_ptr<std::atomic<bool>> readers_running;

    void operator()() {
        for (int version = 2; version <= publications; ++version) {
            config.as_mutable() = make_config(version);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            if (!readers_running->load())
                break;
        }
    }
};

outcome run_copy_on_write() {
    threadsafe::copy_on_write<Config> config{make_config(1)};
    auto highest_version_seen = std::make_shared<std::atomic<int>>(0);
    auto checksum = std::make_shared<std::atomic<std::uint64_t>>(0);
    auto readers_running = std::make_shared<std::atomic<bool>>(true);

    const auto started_at = std::chrono::steady_clock::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(cow_writer_task{config, readers_running});
        for (int reader = 0; reader < reader_count; ++reader)
            launcher.launch_task(
                cow_reader_task{config, highest_version_seen, checksum});
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return {std::chrono::duration<double, std::milli>(elapsed).count(),
            highest_version_seen->load(), checksum->load()};
}

// ---------------------------------------------------------------------------
// 2. synchronized_value<copy_on_write<Config>>: the only publication channel
//    the library actually offers. is_synchronizable<const copy_on_write<T>> is
//    false, so the wrapper picks std::mutex -- every reader takes an EXCLUSIVE
//    lock just to copy the handle out.
// ---------------------------------------------------------------------------
using published_config = threadsafe::synchronized_value<
    threadsafe::copy_on_write<Config>>;

struct published_reader_task {
    std::shared_ptr<published_config> published;
    std::shared_ptr<std::atomic<int>> highest_version_seen;
    std::shared_ptr<std::atomic<std::uint64_t>> checksum;

    void operator()() const {
        std::uint64_t local = 0;
        int local_version = 0;
        for (int read = 0; read < reads_per_reader; ++read) {
            threadsafe::copy_on_write<Config> snapshot = [&] {
                const auto guard = published->lock_shared();
                return *guard;
            }();
            local += consume(*snapshot);
            if (snapshot->version > local_version)
                local_version = snapshot->version;
        }
        *checksum += local;
        int previous = highest_version_seen->load();
        while (previous < local_version
               && !highest_version_seen->compare_exchange_weak(previous,
                                                               local_version))
            ;
    }
};

struct published_writer_task {
    std::shared_ptr<published_config> published;

    void operator()() const {
        for (int version = 2; version <= publications; ++version) {
            {
                auto guard = published->lock();
                guard->as_mutable() = make_config(version);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }
};

outcome run_synchronized_copy_on_write() {
    auto published = published_config::make(
        threadsafe::copy_on_write<Config>{make_config(1)});
    auto highest_version_seen = std::make_shared<std::atomic<int>>(0);
    auto checksum = std::make_shared<std::atomic<std::uint64_t>>(0);

    const auto started_at = std::chrono::steady_clock::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(published_writer_task{published});
        for (int reader = 0; reader < reader_count; ++reader)
            launcher.launch_task(published_reader_task{
                published, highest_version_seen, checksum});
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return {std::chrono::duration<double, std::milli>(elapsed).count(),
            highest_version_seen->load(), checksum->load()};
}

// ---------------------------------------------------------------------------
// 3. synchronized_value<Config>: the wrapper the library should have pointed at.
//    is_synchronizable<const Config> is true, so it really picks shared_mutex.
// ---------------------------------------------------------------------------
using guarded_config = threadsafe::synchronized_value<Config>;

struct guarded_reader_task {
    std::shared_ptr<guarded_config> guarded;
    std::shared_ptr<std::atomic<int>> highest_version_seen;
    std::shared_ptr<std::atomic<std::uint64_t>> checksum;

    void operator()() const {
        std::uint64_t local = 0;
        int local_version = 0;
        for (int read = 0; read < reads_per_reader; ++read) {
            const auto guard = guarded->lock_shared();
            local += consume(*guard);
            if (guard->version > local_version)
                local_version = guard->version;
        }
        *checksum += local;
        int previous = highest_version_seen->load();
        while (previous < local_version
               && !highest_version_seen->compare_exchange_weak(previous,
                                                               local_version))
            ;
    }
};

struct guarded_writer_task {
    std::shared_ptr<guarded_config> guarded;

    void operator()() const {
        for (int version = 2; version <= publications; ++version) {
            {
                auto guard = guarded->lock();
                *guard = make_config(version);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }
};

outcome run_synchronized_value() {
    auto guarded = guarded_config::make(make_config(1));
    auto highest_version_seen = std::make_shared<std::atomic<int>>(0);
    auto checksum = std::make_shared<std::atomic<std::uint64_t>>(0);

    const auto started_at = std::chrono::steady_clock::now();
    {
        threadsafe::asynchronous_task_launcher launcher;
        launcher.launch_task(guarded_writer_task{guarded});
        for (int reader = 0; reader < reader_count; ++reader)
            launcher.launch_task(
                guarded_reader_task{guarded, highest_version_seen, checksum});
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return {std::chrono::duration<double, std::milli>(elapsed).count(),
            highest_version_seen->load(), checksum->load()};
}

// ---------------------------------------------------------------------------
// 4. hand-written std::shared_mutex baseline (no library involved)
// ---------------------------------------------------------------------------
outcome run_shared_mutex_baseline() {
    Config config = make_config(1);
    std::shared_mutex mutex;
    std::atomic<int> highest_version_seen{0};
    std::atomic<std::uint64_t> checksum{0};

    const auto started_at = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> threads;
        threads.emplace_back([&] {
            for (int version = 2; version <= publications; ++version) {
                { std::unique_lock lock{mutex}; config = make_config(version); }
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        });
        for (int reader = 0; reader < reader_count; ++reader)
            threads.emplace_back([&] {
                std::uint64_t local = 0;
                int local_version = 0;
                for (int read = 0; read < reads_per_reader; ++read) {
                    std::shared_lock lock{mutex};
                    local += consume(config);
                    if (config.version > local_version)
                        local_version = config.version;
                }
                checksum += local;
                int previous = highest_version_seen.load();
                while (previous < local_version
                       && !highest_version_seen.compare_exchange_weak(
                              previous, local_version))
                    ;
            });
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return {std::chrono::duration<double, std::milli>(elapsed).count(),
            highest_version_seen.load(), checksum.load()};
}

// ---------------------------------------------------------------------------
// 5. std::atomic<std::shared_ptr<const Config>> baseline. This is the textbook
//    answer to "publish a new config"; the library REJECTS it -- see the
//    static_asserts in main().
// ---------------------------------------------------------------------------
outcome run_atomic_shared_ptr_baseline() {
    std::atomic<std::shared_ptr<const Config>> published{
        std::make_shared<const Config>(make_config(1))};
    std::atomic<int> highest_version_seen{0};
    std::atomic<std::uint64_t> checksum{0};

    const auto started_at = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> threads;
        threads.emplace_back([&] {
            for (int version = 2; version <= publications; ++version) {
                published.store(std::make_shared<const Config>(make_config(version)));
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        });
        for (int reader = 0; reader < reader_count; ++reader)
            threads.emplace_back([&] {
                std::uint64_t local = 0;
                int local_version = 0;
                for (int read = 0; read < reads_per_reader; ++read) {
                    std::shared_ptr<const Config> snapshot = published.load();
                    local += consume(*snapshot);
                    if (snapshot->version > local_version)
                        local_version = snapshot->version;
                }
                checksum += local;
                int previous = highest_version_seen.load();
                while (previous < local_version
                       && !highest_version_seen.compare_exchange_weak(
                              previous, local_version))
                    ;
            });
    }
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return {std::chrono::duration<double, std::milli>(elapsed).count(),
            highest_version_seen.load(), checksum.load()};
}

void report(const char* label, const outcome& result) {
    const double reads = double(reader_count) * reads_per_reader;
    std::printf("%-46s %8.1f ms  %7.0f ns/read  highest version seen by a "
                "reader: %3d %s\n",
                label, result.milliseconds,
                result.milliseconds * 1e6 / reads, result.highest_version_seen,
                result.highest_version_seen <= 1 ? "  <-- NEVER PUBLISHED" : "");
}

}

// What the library says about the textbook publication channels:
static_assert(!threadsafe::is_synchronizable_v<threadsafe::copy_on_write<Config>>,
              "one copy_on_write handle belongs to one thread");
static_assert(!threadsafe::is_sendable_v<std::atomic<std::shared_ptr<const Config>>>,
              "the textbook publish-a-snapshot channel is not even sendable");
static_assert(!threadsafe::is_sendable_v<std::shared_ptr<const Config>>,
              "and neither is the snapshot handle it hands out");
static_assert(threadsafe::is_synchronizable_v<const Config>,
              "although a const Config is exactly what a reader reads");
static_assert(std::is_same_v<published_config::mutex, std::mutex>,
              "readers of a published copy_on_write take an EXCLUSIVE lock");
static_assert(std::is_same_v<guarded_config::mutex, std::shared_mutex>);

int main() {
    std::printf("%d readers x %d reads, writer publishes up to v%d\n\n",
                reader_count, reads_per_reader, publications);
    report("1. copy_on_write, one handle per thread", run_copy_on_write());
    report("2. synchronized_value<copy_on_write<Config>>",
           run_synchronized_copy_on_write());
    report("3. synchronized_value<Config> (shared_mutex)",
           run_synchronized_value());
    report("4. hand-written std::shared_mutex", run_shared_mutex_baseline());
    report("5. std::atomic<std::shared_ptr<const Config>>",
           run_atomic_shared_ptr_baseline());
}
