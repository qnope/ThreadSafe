// as_mutable() copies the SHARED object through a non-const `T&`
// (`std::make_shared<T>(*ptr_)`, and ptr_ is shared_ptr<T>, not
// shared_ptr<const T>).  If T declares both T(T&) and T(const T&), the T&
// overload wins and the detach writes to the object the other threads are
// reading -- although the class documents "a shared T read through const only".
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>
#include <thread>

namespace {
// A type whose copy constructor takes a non-const reference and moves state out
// of the source: the shape of the old std::auto_ptr and of several COM/handle
// wrappers.  `const StealOnCopy` is genuinely safe to read from many threads,
// and it is genuinely safe to send, so the owner vouches for exactly that.
struct StealOnCopy {
    int payload = 0;
    StealOnCopy() = default;
    explicit StealOnCopy(int initial) : payload(initial) {}
    StealOnCopy(const StealOnCopy& other) noexcept : payload(other.payload) {}
    StealOnCopy(StealOnCopy& other) noexcept : payload(other.payload) {
        other.payload = 0;
    }
};
}

template <>
struct threadsafe::is_sendable<StealOnCopy> : std::true_type {};
template <>
struct threadsafe::is_synchronizable<const StealOnCopy> : std::true_type {};

static_assert(threadsafe::is_sendable_v<threadsafe::copy_on_write<StealOnCopy>>);

namespace {
std::atomic<bool> reader_started{false};
std::atomic<bool> writer_done{false};
std::atomic<long long> observed{0};
}

int main() {
    threadsafe::copy_on_write<StealOnCopy> document{StealOnCopy{4242}};
    threadsafe::copy_on_write<StealOnCopy> reader_handle = document;

    std::thread reader([handle = std::move(reader_handle)] {
        reader_started.store(true, std::memory_order_release);
        long long local = 0;
        while (!writer_done.load(std::memory_order_acquire))
            local += handle->payload;      // reads the SHARED object
        observed.store(local, std::memory_order_relaxed);
        std::printf("reader last saw payload=%d\n", handle->payload);
    });

    while (!reader_started.load(std::memory_order_acquire))
        std::this_thread::yield();
    for (int spin = 0; spin != 200; ++spin)
        std::this_thread::yield();

    document.as_mutable().payload = 1;     // detaches: copies through StealOnCopy&
    writer_done.store(true, std::memory_order_release);
    reader.join();
    return 0;
}
