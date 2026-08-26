// Hand extraction of threadsafe::synchronized_value / value_guard with the
// reflection removed, for ThreadSanitizer (GCC has no TSan runtime on arm64
// darwin, Apple clang has no reflection).
//
// Faithfulness: the only edit is that get_mutex_type()'s splice is replaced by
// the mutex it selects for this T.  is_synchronizable_v<const LazySquare> is
// true (proved with g++-16 in probe3_constcast.cpp), so
//   using mutex       = std::shared_mutex;
//   using const_guard = value_guard<const T, std::shared_lock<std::shared_mutex>>;
// Everything else -- the deleted rvalue operators, the const& operators, the
// private (mutex&, T&) constructor, `mutable mutex mutex_; T value_;`,
// `lock()` / `lock_shared()` -- is copied verbatim from
// include/threadsafe/details/synchronized_value.h.
//
// clang++ -std=c++20 -fsanitize=thread -g -O1 -pthread tsan_constcast.cpp -o tsan && ./tsan
#include <atomic>
#include <concepts>
#include <cstdio>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace threadsafe {

template <class T>
class synchronized_value;

template <class T, class Lock>
class value_guard {
public:
    value_guard(const value_guard&) = delete;
    value_guard& operator=(const value_guard&) = delete;

    T& operator*() && noexcept = delete;
    T* operator->() && noexcept = delete;

    T& operator*() const& noexcept { return *value_; }
    T* operator->() const& noexcept { return value_; }

private:
    template <class>
    friend class synchronized_value;

    value_guard(typename Lock::mutex_type& mutex, T& value)
        : lock_(mutex), value_(&value) {}

    Lock lock_;
    T* value_;
};

template <class T>
class synchronized_value {
public:
    using mutex = std::shared_mutex;                       // get_mutex_type() splice

    using guard = value_guard<T, std::unique_lock<mutex>>;
    using const_guard = value_guard<const T, std::shared_lock<mutex>>;

    template <class... Args>
        requires std::constructible_from<T, Args...>
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

struct LazySquare {
    int seed;
    int cache;
    bool cached;

    int square() const {
        if (!cached) {
            const_cast<LazySquare*>(this)->cache = seed * seed;
            const_cast<LazySquare*>(this)->cached = true;
        }
        return cache;
    }
};

int main() {
    for (int attempt = 0; attempt < 200; ++attempt) {
        threadsafe::synchronized_value<LazySquare> shared_value{
            LazySquare{7, 0, false}};

        std::atomic<int> ready{0};
        std::vector<std::thread> readers;
        for (int index = 0; index < 4; ++index)
            readers.emplace_back([&] {
                ready.fetch_add(1);
                while (ready.load() < 4) {}
                const auto reader_guard = shared_value.lock_shared();
                volatile int observed = reader_guard->square();
                (void)observed;
            });

        for (auto& reader : readers)
            reader.join();
    }

    std::printf("done\n");
    return 0;
}
