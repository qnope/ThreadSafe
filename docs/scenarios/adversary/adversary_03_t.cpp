#include <threadsafe/threadsafe.h>
#include <functional>
#include <memory>
#include <vector>

struct Borrower { int* p; };
static_assert(!threadsafe::is_sendable_v<Borrower>);

// [[no_unique_address]]
struct NoUniqueAddress { [[no_unique_address]] Borrower b; int n; };
// anonymous union
struct AnonUnion { union { int i; int* p; }; };
// unnamed member type
struct UnnamedMemberType { struct { int* p; } nested; };
// virtual base
struct VBase { int* p; };
struct VDerivedA : virtual VBase {};
struct VDerivedB : virtual VBase {};
struct VJoin : VDerivedA, VDerivedB {};
// private base
struct PrivateBase : private Borrower {};
// bitfield next to a borrow
struct BitfieldMix { unsigned a : 3; unsigned b : 5; int* p; };
// array of borrows
struct ArrayOfBorrows { Borrower b[4]; };
// member of anonymous-namespace-less unnamed enum
enum { AnonEnumerator };
static_assert(!threadsafe::is_sendable_v<ArrayOfBorrows>, "ArrayOfBorrows");
