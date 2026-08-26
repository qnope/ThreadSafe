// TSan check of synchronized_value's runtime body.
// Extracted verbatim from include/threadsafe/details/synchronized_value.h with
// the two consteval splices resolved by hand for T = std::map<int,int>:
//   is_synchronizable_v<const std::map<int,int>> is true, so
//     using mutex       = std::shared_mutex;
//     using const_guard = value_guard<const T, std::shared_lock<mutex>>;
// Everything else -- the members, the constructor, lock(), lock_shared(),
// operator*, operator-> -- is copied line for line.
//
// build: clang++ -std=c++20 -fsanitize=thread -g -O1 -pthread \
//            tsan_synchronized_value.cpp -o tsan_sv && ./tsan_sv
#include <cstdint>
#include <cstdio>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <vector>

namespace threadsafe {

template <class T, class Lock>
class value_guard {
public:
    value_guard(const value_guard&) = delete;
    value_guard& operator=(const value_guard&) = delete;

    T& operator*() const& noexcept { return *value_; }
    T* operator->() const& noexcept { return value_; }

    value_guard(typename Lock::mutex_type& mutex, T& value)
        : lock_(mutex), value_(&value) {}

private:
    Lock lock_;
    T* value_;
};

template <class T>
class synchronized_value {
public:
    using mutex = std::shared_mutex;                              // spliced
    using guard = value_guard<T, std::unique_lock<mutex>>;
    using const_guard = value_guard<const T, std::shared_lock<mutex>>;  // spliced

    template <class... Args>
    explicit synchronized_value(Args&&... args)
        : value_(std::forward<Args>(args)...) {}

    synchronized_value(const synchronized_value&) = delete;
    synchronized_value& operator=(const synchronized_value&) = delete;

    [[nodiscard]] guard lock() { return guard{mutex_, value_}; }
    [[nodiscard]] const_guard lock_shared() const {
        return const_guard{mutex_, value_};
    }

private:
    mutable mutex mutex_;
    T value_;
};

}

namespace {
constexpr int operations_per_thread = 20'000;
constexpr int key_space = 512;
constexpr int thread_count = 8;

struct fast_random {
    std::uint64_t state;
    std::uint64_t next() {
        state ^= state << 13; state ^= state >> 7; state ^= state << 17;
        return state;
    }
};
}

int main() {
    threadsafe::synchronized_value<std::map<int, int>> table;
    std::atomic<std::uint64_t> checksum{0};
    {
        auto guard = table.lock();
        for (int key = 0; key < key_space; key += 2) (*guard)[key] = key * 2;
    }
    {
        std::vector<std::jthread> threads;
        for (int worker = 0; worker < thread_count; ++worker)
            threads.emplace_back([&table, &checksum, worker] {
                fast_random random{std::uint64_t(worker) * 2654435761u + 1};
                std::uint64_t local = 0;
                for (int op = 0; op < operations_per_thread; ++op) {
                    const std::uint64_t draw = random.next();
                    const int key = int(draw % key_space);
                    if (draw % 10 != 0) {
                        const auto guard = table.lock_shared();
                        const auto found = guard->find(key);
                        if (found != guard->end())
                            local += std::uint64_t(found->second);
                    } else {
                        auto guard = table.lock();
                        (*guard)[key] = key * 2;
                    }
                }
                checksum += local;
            });
    }
    const auto guard = table.lock_shared();
    std::printf("ok: %zu entries, checksum %llu\n", guard->size(),
                (unsigned long long)checksum.load());
}
