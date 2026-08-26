// Hand-extracted copy of threadsafe::copy_on_write<T> -- the class body is
// verbatim from include/threadsafe/details/copy_on_write.h lines 15-41; only
// the trait specializations (which are compile-time only) were dropped, and
// the acquire fence is put behind THREADSAFE_COW_FENCE so both variants build.
//
// build:  clang++ -std=c++20 -fsanitize=thread -g -O1 -pthread tsan_cow.cpp -o prog
//         clang++ -std=c++20 -DTHREADSAFE_COW_FENCE -fsanitize=thread -g -O1 -pthread tsan_cow.cpp -o prog_fence
#include <atomic>
#include <concepts>
#include <cstdio>
#include <memory>
#include <thread>
#include <type_traits>
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
            ptr_ = std::make_shared<T>(*ptr_);
#ifdef THREADSAFE_COW_FENCE
        else
            std::atomic_thread_fence(std::memory_order_acquire);
#endif
        return *ptr_;
    }

private:
    std::shared_ptr<T> ptr_;
};

}

struct Data {
    int values[64] = {};
};

// A relaxed flag: it orders the two threads in real time but creates NO
// happens-before edge, so the only candidate edge left is the refcount.
static std::atomic<int> phase{0};

// SCENARIO A -- "concurrent detach".
// Both threads hold their own copy_on_write object sharing one control block.
// Thread 1 detaches: as_mutable() READS the shared Data (to copy it) and then
// decrements the count 2 -> 1. Thread 2 then sees use_count() == 1 and WRITES
// that same Data in place.
static void scenario_concurrent_detach() {
    threadsafe::copy_on_write<Data> writer(Data{});
    threadsafe::copy_on_write<Data> other = writer;   // use_count() == 2
    phase.store(0, std::memory_order_relaxed);

    std::thread detacher([&] {
        writer.as_mutable().values[0] = 1;            // reads shared Data, then 2->1
        phase.store(1, std::memory_order_relaxed);
    });
    std::thread in_place([&] {
        while (phase.load(std::memory_order_relaxed) != 1) {}
        other.as_mutable().values[0] = 2;             // sees 1, writes in place
    });
    detacher.join();
    in_place.join();
}

// SCENARIO B -- "handoff by destruction", the shape the commit message names.
// Thread 1 READS the shared Data through the const interface and then destroys
// its handle (2 -> 1). Thread 2, which received its copy before that, sees
// use_count() == 1 and writes in place.
static void scenario_handoff() {
    auto shared_handle = std::make_unique<threadsafe::copy_on_write<Data>>(Data{});
    threadsafe::copy_on_write<Data> receiver = *shared_handle;   // use_count() == 2
    phase.store(0, std::memory_order_relaxed);

    std::thread reader([&] {
        int seen = (**shared_handle).values[0];       // plain read of the shared Data
        asm volatile("" :: "r"(seen) : "memory");
        shared_handle.reset();                        // release decrement 2 -> 1
        phase.store(1, std::memory_order_relaxed);
    });
    std::thread writer([&] {
        while (phase.load(std::memory_order_relaxed) != 1) {}
        receiver.as_mutable().values[0] = 7;          // sees 1, writes in place
    });
    reader.join();
    writer.join();
}

int main(int argc, char**) {
    for (int iteration = 0; iteration < 200; ++iteration) {
        scenario_concurrent_detach();
        scenario_handoff();
    }
    std::printf("done (%d)\n", argc);
}
