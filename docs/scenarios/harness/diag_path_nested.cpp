// Backs docs/04-diagnostics.md: the path carried by assert_* names the root
// cause instead of the first hop. Every function below is a *deliberate*
// compile error — the message it produces is the point.
#include <threadsafe/threadsafe.h>

#include <vector>

struct Inner { int *borrowed; };
struct Middle { Inner inner; };
struct Outer { Middle middle; };

struct Base { int *borrowed; };
struct Derived : Base { int ok; };

struct Cells { Inner cells[3]; };

struct SyncLeaf { mutable int cached; };
struct SyncMid { SyncLeaf leaf; };
struct SyncTop { SyncMid mid; };

struct Vec { std::vector<int *> values; };

struct Handle { int value; };

template <>
struct threadsafe::is_sendable<Handle> : std::false_type {};

struct HoldsHandle { Handle handle; };

// Outer::middle (Middle)::inner (Inner)::borrowed (int*) is a pointer or a
// reference: sending it shares its referent with the other thread, …
void nested_send() { threadsafe::assert_sendable<Outer>(); }

// Outer::middle (Middle)::inner (Inner)::borrowed (int*) is a reference or a
// raw pointer: it borrows its referent instead of keeping it alive, …
void nested_life() { threadsafe::assert_lifetime_aware<Outer>(); }

// const SyncTop::mid (SyncMid)::leaf (SyncLeaf)::cached (int) is mutable, so it
// is written through a const reference: its type must be fully synchronizable
void nested_sync() { threadsafe::assert_synchronizable<const SyncTop>(); }

// A base is named as one: Derived::(base Base)::borrowed (int*) is a pointer …
void through_base() { threadsafe::assert_sendable<Derived>(); }

// An array adds no step: Cells::cells (Inner [3])::borrowed (int*) is a pointer …
void through_array() { threadsafe::assert_sendable<Cells>(); }

// A specialization says so: HoldsHandle::handle (Handle) is not sendable:
// is_sendable is specialized to false for it
void specialized() { threadsafe::assert_sendable<HoldsHandle>(); }

// The limit kept from docs/08-api-et-flexibilite.md section 3b: the walk enters
// std::vector and answers with its own structural reason, not the one the
// specialization of containers.h actually used.
// Vec::values (std::vector<int*>) has a user-written copy, move or destructor …
void specialized_std() { threadsafe::assert_sendable<Vec>(); }
