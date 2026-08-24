// Three write paths through a const object that the structural walk cannot see,
// because it inspects DATA only.  All three types have nothing but value data
// members and nothing but implicit special members, so the walk accepts them.

#include <threadsafe/threadsafe.h>

#include <cstdio>

namespace {

int shared_slab[64];

// (a) a plain int member used as an index into storage the object does not own
struct SlabHandle {
    int index;
    void bump() const { ++shared_slab[index]; }
};

// (b) the textbook "logical const" cache, written with const_cast instead of
//     mutable -- the walk sees only two ints
struct LazyParse {
    int raw;
    int parsed;
    int value() const {
        return const_cast<LazyParse *>(this)->parsed = raw * 2;
    }
};

// (c) a conversion operator that launders const away
struct WritableFacade {
    int cell;
    operator int &() const { return const_cast<int &>(cell); }
};

// (d) a const member function returning a non-const reference to a member
struct LeakyAccessor {
    int cell;
    int &mutable_cell() const { return const_cast<int &>(cell); }
};

}

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

static_assert(is_synchronizable_v<const SlabHandle>);
static_assert(is_synchronizable_v<const LazyParse>);
static_assert(is_synchronizable_v<const WritableFacade>);
static_assert(is_synchronizable_v<const LeakyAccessor>);

static_assert(is_sendable_v<SlabHandle> && is_sendable_v<LazyParse>
              && is_sendable_v<WritableFacade>
              && is_sendable_v<LeakyAccessor>);

int main() {
    const SlabHandle handle{3};
    handle.bump();

    const LazyParse parse{21, 0};
    (void)parse.value();

    const WritableFacade facade{0};
    static_cast<int &>(facade) = 7;

    const LeakyAccessor accessor{0};
    accessor.mutable_cell() = 9;

    std::printf("all four const objects were written through: %d %d %d %d\n",
                shared_slab[3], parse.parsed, facade.cell, accessor.cell);
    return 0;
}
