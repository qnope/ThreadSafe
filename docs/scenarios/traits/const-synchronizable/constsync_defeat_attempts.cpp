// Attempts to defeat the three explicit rules -- mutable, reference, pointer --
// through shapes reflection might not report the same way.
#include <threadsafe/threadsafe.h>

#include <atomic>
#include <cstdio>

namespace {

struct MutableInAnonymousUnion {
    union { mutable int cache; float other; };
};

union MutableInsideUnion {
    mutable int cache;
    float other;
};

struct ReferenceInAnonymousUnion {   // unions cannot hold references; use a base
    int value;
};
struct BaseWithReference { int &borrowed; };
struct DerivesFromReferenceHolder : BaseWithReference {};

struct PrivateMutable {
private:
    mutable int cache;
};

struct PrivatePointer {
private:
    int *borrowed;
};

struct VirtualBaseWithPointer { int *borrowed; };
struct LeftVirtual : virtual VirtualBaseWithPointer {};
struct RightVirtual : virtual VirtualBaseWithPointer {};
struct Diamond : LeftVirtual, RightVirtual {};

struct StaticOnly { static inline int shared_counter = 0; int payload; };
struct ThreadLocalOnly { static thread_local int per_thread; int payload; };
thread_local int ThreadLocalOnly::per_thread = 0;

struct BitFields { unsigned a : 1; unsigned b : 31; };

struct MutableArrayOfAtomics { mutable std::atomic<int> counters[4]; };
struct MutableArrayOfInts { mutable int counters[4]; };

struct ConstPointerMember { int *const borrowed; };
struct PointerArrayMember { int *borrowed[4]; };
struct MemberPointerMember { int StaticOnly::*offset; };
struct MemberFunctionPointerMember { void (StaticOnly::*method)(); };
struct RvalueReferenceMember { int &&borrowed; };
struct VolatileMember { volatile int cell; };
struct NestedPimplLike { struct Hidden; Hidden *hidden; };

}

#define SHOW(...) std::printf("%-40s const-sync=%d\n", #__VA_ARGS__, \
        (int)threadsafe::is_synchronizable_v<const __VA_ARGS__>)

int main() {
    SHOW(MutableInAnonymousUnion);
    SHOW(MutableInsideUnion);
    SHOW(DerivesFromReferenceHolder);
    SHOW(PrivateMutable);
    SHOW(PrivatePointer);
    SHOW(Diamond);
    SHOW(StaticOnly);
    SHOW(ThreadLocalOnly);
    SHOW(BitFields);
    SHOW(MutableArrayOfAtomics);
    SHOW(MutableArrayOfInts);
    SHOW(ConstPointerMember);
    SHOW(PointerArrayMember);
    SHOW(MemberPointerMember);
    SHOW(MemberFunctionPointerMember);
    SHOW(RvalueReferenceMember);
    SHOW(VolatileMember);
    SHOW(NestedPimplLike);
    return 0;
}
