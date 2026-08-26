// TSan check of copy_on_write's runtime body, which is already plain C++.
// Copied line for line from include/threadsafe/details/copy_on_write.h:
//     const T& operator*() const noexcept { return *ptr_; }
//     const T* operator->() const noexcept { return ptr_.get(); }
//     T& as_mutable() requires std::copy_constructible<T> {
//         if (ptr_.use_count() != 1) ptr_ = std::make_shared<T>(*ptr_);
//         else std::atomic_thread_fence(std::memory_order_acquire);
//         return *ptr_;
//     }
//     std::shared_ptr<T> ptr_;
//
// PART 1 is the pattern the traits bless (one handle per thread, copied at
// spawn -- is_sendable<copy_on_write<T>> is true).
// PART 2 is the pattern the traits refuse (one handle shared by reference --
// is_synchronizable<copy_on_write<T>> is false); it is here to check that the
// refusal is protecting against a real race and not a phantom.
//
// build: clang++ -std=c++20 -fsanitize=thread -g -O1 -pthread \
//            tsan_copy_on_write.cpp -o tsan_cow && ./tsan_cow 1   (blessed)
//                                                  ./tsan_cow 2   (refused)
#include <atomic>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace threadsafe {

template <class T>
class copy_on_write {
public:
    template <class... Args>
        requires std::constructible_from<T, Args...>
              && (sizeof...(Args) != 1
                  || (!std::same_as<std::remove_cvref_t<Args>, copy_on_write>
                      && ...))
    explicit copy_on_write(Args&&... args)
        : ptr_(std::make_shared<T>(std::forward<Args>(args)...)) {}

    const T& operator*() const noexcept { return *ptr_; }
    const T* operator->() const noexcept { return ptr_.get(); }

    T& as_mutable()
        requires std::copy_constructible<T>
    {
        if (ptr_.use_count() != 1)
            ptr_ = std::make_shared<T>(*ptr_);
        else
            std::atomic_thread_fence(std::memory_order_acquire);
        return *ptr_;
    }

private:
    std::shared_ptr<T> ptr_;
};

}

namespace {
struct Config { std::string name; std::vector<int> weights; int version; };

Config make_config(int version) {
    return Config{"config-v" + std::to_string(version),
                  std::vector<int>(32, version), version};
}

constexpr int reader_count = 8;
constexpr int reads_per_reader = 50'000;
constexpr int publications = 500;

void blessed_one_handle_per_thread() {
    threadsafe::copy_on_write<Config> origin{make_config(1)};
    std::atomic<std::uint64_t> checksum{0};
    std::atomic<int> highest_seen{0};
    {
        std::vector<std::jthread> threads;
        threads.emplace_back([writer = origin]() mutable {
            for (int version = 2; version <= publications; ++version)
                writer.as_mutable() = make_config(version);
        });
        for (int reader = 0; reader < reader_count; ++reader)
            threads.emplace_back([handle = origin, &checksum, &highest_seen] {
                std::uint64_t local = 0;
                int local_version = 0;
                for (int read = 0; read < reads_per_reader; ++read) {
                    local += handle->name.size();
                    for (int weight : handle->weights) local += unsigned(weight);
                    if (handle->version > local_version)
                        local_version = handle->version;
                }
                checksum += local;
                int previous = highest_seen.load();
                while (previous < local_version
                       && !highest_seen.compare_exchange_weak(previous,
                                                              local_version))
                    ;
            });
    }
    std::printf("blessed pattern: checksum %llu, highest version any reader "
                "saw = %d\n",
                (unsigned long long)checksum.load(), highest_seen.load());
}

void refused_one_shared_handle() {
    threadsafe::copy_on_write<Config> shared{make_config(1)};
    std::atomic<std::uint64_t> checksum{0};
    {
        std::vector<std::jthread> threads;
        threads.emplace_back([&shared] {
            for (int version = 2; version <= publications; ++version)
                shared.as_mutable() = make_config(version);
        });
        for (int reader = 0; reader < reader_count; ++reader)
            threads.emplace_back([&shared, &checksum] {
                std::uint64_t local = 0;
                for (int read = 0; read < reads_per_reader; ++read)
                    local += shared->name.size();
                checksum += local;
            });
    }
    std::printf("refused pattern: checksum %llu\n",
                (unsigned long long)checksum.load());
}
}

int main(int argc, char** argv) {
    const int part = argc > 1 ? std::atoi(argv[1]) : 1;
    if (part == 1) blessed_one_handle_per_thread();
    else refused_one_shared_handle();
}
