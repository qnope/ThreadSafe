// Crown-jewel probe: a struct whose ONLY data member is an int -- no mutable
// member, no reference member, no pointer member -- still hands every reader a
// writer path, because the write lives in a const member function that reaches
// storage the object does not contain.
//
// is_synchronizable_v<const SlabHandle> is true, so synchronized_value picks a
// std::shared_mutex and lock_shared() lets N readers hold a const SlabHandle&
// at the same time. Both of them then write the same int.

#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <thread>

namespace {

int slab[64];

struct SlabHandle {
    int index;

    void bump() const { ++slab[index]; }
    int read() const { return slab[index]; }
};

}

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(is_sendable_v<SlabHandle>,
              "every library check passes: the handle is sendable");
static_assert(is_synchronizable_v<const SlabHandle>,
              "every library check passes: the library calls a const SlabHandle "
              "readable from several threads at once");
static_assert(std::is_same_v<threadsafe::synchronized_value<SlabHandle>::mutex,
                             std::shared_mutex>,
              "so synchronized_value picks a shared_mutex ...");
static_assert(
    std::is_same_v<
        threadsafe::synchronized_value<SlabHandle>::const_guard,
        threadsafe::value_guard<const SlabHandle,
                                std::shared_lock<std::shared_mutex>>>,
    "... and lock_shared() takes it in shared mode");

int main() {
    threadsafe::synchronized_value<SlabHandle> shared_handle{SlabHandle{7}};

    auto hammer = [&shared_handle] {
        for (int iteration = 0; iteration < 20000; ++iteration) {
            const auto reader_guard = shared_handle.lock_shared();
            reader_guard->bump();
        }
    };

    std::thread first_reader{hammer};
    std::thread second_reader{hammer};
    first_reader.join();
    second_reader.join();

    const auto reader_guard = shared_handle.lock_shared();
    std::printf("slab[7] = %d (expected 40000)\n", reader_guard->read());
    return 0;
}
