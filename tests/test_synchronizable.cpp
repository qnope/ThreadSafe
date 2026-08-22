#include <threadsafe/synchronizable.h>

#include <atomic>

namespace {
struct Plain {};
struct SyncType {};
struct OneInt {
    int i;
};
struct UserCopy {
    UserCopy(const UserCopy&);
};
struct MutInt {
    mutable int i;
};
struct SafeCounter {
    mutable std::atomic<int> hits;
};
struct HoldsPtr {
    int* p;
};
struct HoldsCString {
    const char* name;
};
struct PtrToAtomic {
    std::atomic<int>* c;
};
struct HoldsFnPtr {
    void (*f)();
};
struct HoldsRef {
    int& r;
};
struct HoldsConstRef {
    const int& r;
};
struct RefsAtomic {
    std::atomic<int>& a;
};
struct Node {
    Node* next;
    int v;
};
struct PList {
    int v;
    const PList* tail;
};
struct ImmutableNode {
    const ImmutableNode* next;
};
}

template <>
constexpr bool threadsafe::is_synchronizable<SyncType> = true;

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(const ImmutableNode);

using threadsafe::is_synchronizable;

static_assert(!is_synchronizable<int>,
              "is_synchronizable — default is false");
static_assert(!is_synchronizable<Plain>,
              "is_synchronizable — default is false for class types");

static_assert(is_synchronizable<void()>,
              "is_synchronizable — function types are synchronizable");
static_assert(is_synchronizable<int(int) noexcept>,
              "is_synchronizable — function types are synchronizable");

static_assert(is_synchronizable<SyncType>,
              "is_synchronizable — explicit specialization beats the default");

static_assert(is_synchronizable<std::atomic<int>>,
              "is_synchronizable — atomic of a sendable type is synchronizable");
static_assert(!is_synchronizable<std::atomic<int*>>,
              "is_synchronizable — atomic of a non-sendable type is not");
static_assert(is_synchronizable<std::atomic<SyncType*>>,
              "is_synchronizable — atomic follows is_sendable of its value type");
static_assert(threadsafe::is_sendable<std::atomic<int>>,
              "is_sendable — a synchronizable atomic is sendable via rule 1");
static_assert(threadsafe::is_sendable<std::atomic<int>&>,
              "is_sendable — a reference to a synchronizable atomic is sendable");
static_assert(!threadsafe::is_sendable<std::atomic<int*>>,
              "is_sendable — an atomic follows the sendability of its value type");

static_assert(is_synchronizable<const int>,
              "is_synchronizable — concurrent reads of a scalar are safe");
static_assert(is_synchronizable<const volatile int> && !is_synchronizable<volatile int>,
              "is_synchronizable — volatile does not add a writer, the missing "
              "const does");
static_assert(is_synchronizable<const std::atomic<int>>,
              "is_synchronizable — full synchronizability implies the read-only "
              "form");
static_assert(!is_synchronizable<const std::atomic<int*>>,
              "is_synchronizable — an atomic that is not synchronizable is not "
              "rescued by const: load() const hands out the pointer");
static_assert(is_synchronizable<const Plain> && is_synchronizable<const OneInt>,
              "is_synchronizable — plain data read through const has nothing to "
              "race on");
static_assert(!is_synchronizable<const UserCopy>,
              "is_synchronizable — the structural guard is the same as "
              "is_sendable's: a user-provided special member hides what the "
              "recursion cannot see");
static_assert(!is_synchronizable<const MutInt>,
              "is_synchronizable — mutable turns a const method into a writer "
              "under the readers");
static_assert(is_synchronizable<const SafeCounter>,
              "is_synchronizable — a mutable member is fine when its own type "
              "handles the concurrency");
static_assert(!is_synchronizable<int* const> && !is_synchronizable<const int* const>,
              "is_synchronizable — a pointee's const is a view restriction, not "
              "an object property: it may have been reached non-const at origin");
static_assert(!is_synchronizable<const HoldsPtr> && !is_synchronizable<const HoldsCString>,
              "is_synchronizable — a pointer member gives every reader a write "
              "path, and a pointed-to const proves nothing about other aliases");
static_assert(is_synchronizable<std::atomic<int>* const>
                  && is_synchronizable<const PtrToAtomic>,
              "is_synchronizable — a fully synchronizable pointee survives the "
              "sharing that copying the pointer amounts to");
static_assert(is_synchronizable<const HoldsFnPtr>,
              "is_synchronizable — a function pointee is code, and code is "
              "immutable");
static_assert(!is_synchronizable<const HoldsRef> && !is_synchronizable<const HoldsConstRef>,
              "is_synchronizable — the const does not travel through a "
              "reference member: the referent may be aliased non-const "
              "elsewhere");
static_assert(is_synchronizable<const RefsAtomic>,
              "is_synchronizable — a referent that handles its own concurrency "
              "needs no const");
static_assert(is_synchronizable<const decltype([] {})>,
              "is_synchronizable — a captureless lambda is empty, nothing to "
              "race on");
static_assert(!is_synchronizable<const decltype([x = 42] {})>,
              "is_synchronizable — a closure reflects no members, so its "
              "captures are state the recursion cannot inspect");
static_assert(is_synchronizable<std::atomic<int>[4]> && is_synchronizable<const int[4]>,
              "is_synchronizable — owned storage follows its element type");
static_assert(!is_synchronizable<const Node> && !is_synchronizable<const PList>,
              "is_synchronizable — a self-pointer asks the full trait of its "
              "pointee, so the recursion terminates instead of chasing itself");
static_assert(is_synchronizable<const ImmutableNode>,
              "is_synchronizable — the macro on a const type asserts read-only "
              "sharing; the full specialization outranks the partial one");
