#include <threadsafe/threadsafe.h>
#include <atomic>
#include <cstddef>

namespace {
struct Sync {};
struct Incomplete;
struct Plain { int a; void f(); };
}
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Sync);

using threadsafe::is_sendable_v;

// --- function pointers
static_assert(is_sendable_v<void (*)()>);
static_assert(is_sendable_v<void (*)(int, ...)>);
static_assert(is_sendable_v<void (*)() noexcept>);
static_assert(is_sendable_v<void (*const)()>);
static_assert(is_sendable_v<void (*const volatile)()>);
static_assert(!is_sendable_v<void (**)()>);          // pointer to function pointer?
static_assert(is_sendable_v<void (&)()>);           // reference to function

// --- pointer to member
static_assert(is_sendable_v<int Plain::*>);
static_assert(is_sendable_v<void (Plain::*)()>);
static_assert(is_sendable_v<int Plain::*const>);

// --- void*
static_assert(!is_sendable_v<void*>);
static_assert(!is_sendable_v<const void*>);
static_assert(!is_sendable_v<void* const>);

// --- pointer to array
static_assert(!is_sendable_v<int (*)[4]>);
static_assert(is_sendable_v<std::atomic<int> (*)[4]>);
static_assert(is_sendable_v<const std::atomic<int> (*)[4]>);
static_assert(is_sendable_v<std::atomic<int> (*)[]>);

// --- pointer to pointer
static_assert(!is_sendable_v<int**>);
static_assert(!is_sendable_v<std::atomic<int>**>);
static_assert(is_sendable_v<Sync*>);
static_assert(!is_sendable_v<Sync**>);

// --- incomplete
static_assert(!is_sendable_v<Incomplete*>);
static_assert(!is_sendable_v<Incomplete**>);

// --- cv on the pointer vs on the pointee
static_assert(!is_sendable_v<int* const>);
static_assert(!is_sendable_v<const int*>);
static_assert(!is_sendable_v<const int* const volatile>);
static_assert(!is_sendable_v<volatile int*>);
static_assert(is_sendable_v<Sync* const>);
static_assert(is_sendable_v<const Sync*>);
static_assert(is_sendable_v<Sync* const volatile>);
static_assert(is_sendable_v<volatile Sync*>);

// --- nullptr_t
static_assert(is_sendable_v<std::nullptr_t>);
