// Isolates the acquire fence.  Main waits (on a RELAXED flag, which creates no
// happens-before of its own) until the worker has dropped its handle, so
// as_mutable() is guaranteed to take the in-place branch.  The only edge from
// the worker's reads to main's writes is:
//   worker's non-last release decrement (acq_rel RMW)
//     -> main's RELAXED use_count() load reading 1
//        -> main's std::atomic_thread_fence(memory_order_acquire).
#include <concepts>
#include <memory>
#include <utility>

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
            ptr_ = std::make_shared<T>(std::as_const(*ptr_));
        else
            acquire_the_block();
        return *ptr_;
    }

private:
    // The last handle other than ours released the block with an acq_rel RMW on
    // the reference count; use_count() reads it relaxed, so the acquire has to
    // be supplied here ([atomics.fences]/3). ThreadSanitizer does not model
    // std::atomic_thread_fence at all, so under -fsanitize=thread the same
    // acquire is taken through a real RMW instead.
    void acquire_the_block() noexcept {
#ifdef __SANITIZE_THREAD__
        [[maybe_unused]] const std::shared_ptr<T> acquiring_copy = ptr_;
#else
        std::atomic_thread_fence(std::memory_order_acquire);
#endif
    }

    std::shared_ptr<T> ptr_;
};
}

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

using threadsafe::copy_on_write;

namespace {
struct Payload {
    std::vector<int> counters;
};

std::atomic<bool> handle_dropped{false};
std::atomic<bool> may_finish{false};
std::atomic<long long> observed_sum{0};
}

int main() {
    copy_on_write<Payload> document{Payload{std::vector<int>(256, 7)}};
    copy_on_write<Payload> worker_handle = document;

    std::thread worker([handle = std::move(worker_handle)]() mutable {
        long long local_sum = 0;
        for (int value : handle->counters)
            local_sum += value;
        observed_sum.store(local_sum, std::memory_order_relaxed);
        handle = copy_on_write<Payload>{Payload{}};
        handle_dropped.store(true, std::memory_order_relaxed);
        while (!may_finish.load(std::memory_order_relaxed))
            std::this_thread::yield();
    });

    while (!handle_dropped.load(std::memory_order_relaxed))
        std::this_thread::yield();

    const Payload* block_before = document.operator->();
    Payload& writable = document.as_mutable();
    std::printf("in_place=%d\n", &writable == block_before);
    for (int& counter : writable.counters)
        counter = 99;

    std::printf("sum=%lld first=%d\n",
                observed_sum.load(std::memory_order_relaxed),
                document->counters[0]);
    may_finish.store(true, std::memory_order_relaxed);
    worker.join();
    return 0;
}
