#include <threadsafe/threadsafe.h>
#include <atomic>
#include <string>

namespace {
struct Sync {};
struct UserCopy { UserCopy(const UserCopy&); };
struct Flexible { unsigned n; int data[]; };
struct WithZero { int z[0]; int v; };
}
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Sync);

using threadsafe::is_sendable_v;

static_assert(is_sendable_v<int[4]>);
static_assert(is_sendable_v<int[]>);
static_assert(is_sendable_v<const int[4]>);
static_assert(is_sendable_v<volatile int[4]>);
static_assert(is_sendable_v<const volatile int[4]>);
static_assert(is_sendable_v<const int[]>);

// multi-dimensional
static_assert(is_sendable_v<int[2][3]>);
static_assert(is_sendable_v<int[2][3][4]>);
static_assert(is_sendable_v<const int[2][3]>);
static_assert(!is_sendable_v<int*[2][3]>);
static_assert(!is_sendable_v<UserCopy[2][3]>);
static_assert(is_sendable_v<int[][3]>);
static_assert(!is_sendable_v<int*[][3]>);

// element types
static_assert(!is_sendable_v<int*[4]>);
static_assert(is_sendable_v<Sync*[4]>);
static_assert(!is_sendable_v<UserCopy[4]>);
static_assert(!is_sendable_v<const UserCopy[4]>);
static_assert(is_sendable_v<std::string[4]>);
static_assert(is_sendable_v<std::atomic<int>[4]>);

// zero-length (GNU extension) and flexible array member
static_assert(is_sendable_v<int[0]>);
static_assert(is_sendable_v<WithZero>);
static_assert(is_sendable_v<Flexible>);

// array of unknown bound as a member is a GNU flexible array member; array of
// pointers as one:
namespace { struct FlexibleBorrow { unsigned n; int* data[]; }; }
static_assert(!is_sendable_v<FlexibleBorrow>);

// arrays inside structs
namespace { struct Buf { char b[64]; }; struct BadBuf { UserCopy b[2]; }; }
static_assert(is_sendable_v<Buf>);
static_assert(!is_sendable_v<BadBuf>);
