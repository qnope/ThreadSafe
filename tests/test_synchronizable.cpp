#include <threadsafe/threadsafe.h>

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
struct threadsafe::is_unsafe_synchronizable<SyncType> : std::true_type {};

template <>
struct threadsafe::is_unsafe_synchronizable<const ImmutableNode> : std::true_type {};

using threadsafe::is_synchronizable_v;

static_assert(!is_synchronizable_v<int>,
              "is_synchronizable — default is false");
static_assert(!is_synchronizable_v<Plain>,
              "is_synchronizable — default is false for class types");

static_assert(is_synchronizable_v<void()>,
              "is_synchronizable — function types are synchronizable");
static_assert(is_synchronizable_v<int(int) noexcept>,
              "is_synchronizable — function types are synchronizable");

static_assert(is_synchronizable_v<SyncType>,
              "is_synchronizable — explicit specialization beats the default");

static_assert(is_synchronizable_v<std::atomic<int>>,
              "is_synchronizable — atomic of a sendable type is synchronizable");
static_assert(!is_synchronizable_v<std::atomic<int*>>,
              "is_synchronizable — atomic of a non-sendable type is not");
static_assert(is_synchronizable_v<std::atomic<SyncType*>>,
              "is_synchronizable — atomic follows is_sendable of its value type");
static_assert(threadsafe::is_sendable_v<std::atomic<int>>,
              "is_sendable — a synchronizable atomic is sendable via rule 1");
static_assert(threadsafe::is_sendable_v<std::atomic<int>&>,
              "is_sendable — a reference to a synchronizable atomic is sendable");
static_assert(!threadsafe::is_sendable_v<std::atomic<int*>>,
              "is_sendable — an atomic follows the sendability of its value type");

static_assert(is_synchronizable_v<const int>,
              "is_synchronizable — concurrent reads of a scalar are safe");
static_assert(is_synchronizable_v<const volatile int> && !is_synchronizable_v<volatile int>,
              "is_synchronizable — volatile does not add a writer, the missing "
              "const does");
static_assert(is_synchronizable_v<const std::atomic<int>>,
              "is_synchronizable — full synchronizability implies the read-only "
              "form");
static_assert(!is_synchronizable_v<const std::atomic<int*>>,
              "is_synchronizable — an atomic that is not synchronizable is not "
              "rescued by const: load() const hands out the pointer");
static_assert(is_synchronizable_v<const Plain> && is_synchronizable_v<const OneInt>,
              "is_synchronizable — plain data read through const has nothing to "
              "race on");
static_assert(!is_synchronizable_v<const UserCopy>,
              "is_synchronizable — the structural guard is the same as "
              "is_sendable's: a user-provided special member hides what the "
              "recursion cannot see");
static_assert(!is_synchronizable_v<const MutInt>,
              "is_synchronizable — mutable turns a const method into a writer "
              "under the readers");
static_assert(is_synchronizable_v<const SafeCounter>,
              "is_synchronizable — a mutable member is fine when its own type "
              "handles the concurrency");
static_assert(!is_synchronizable_v<int* const> && !is_synchronizable_v<const int* const>,
              "is_synchronizable — a pointee's const is a view restriction, not "
              "an object property: it may have been reached non-const at origin");
static_assert(!is_synchronizable_v<const HoldsPtr> && !is_synchronizable_v<const HoldsCString>,
              "is_synchronizable — a pointer member gives every reader a write "
              "path, and a pointed-to const proves nothing about other aliases");
static_assert(is_synchronizable_v<std::atomic<int>* const>
                  && is_synchronizable_v<const PtrToAtomic>,
              "is_synchronizable — a fully synchronizable pointee survives the "
              "sharing that copying the pointer amounts to");
static_assert(is_synchronizable_v<const HoldsFnPtr>,
              "is_synchronizable — a function pointee is code, and code is "
              "immutable");
static_assert(!is_synchronizable_v<const HoldsRef> && !is_synchronizable_v<const HoldsConstRef>,
              "is_synchronizable — the const does not travel through a "
              "reference member: the referent may be aliased non-const "
              "elsewhere");
static_assert(is_synchronizable_v<const RefsAtomic>,
              "is_synchronizable — a referent that handles its own concurrency "
              "needs no const");
static_assert(is_synchronizable_v<const decltype([] {})>,
              "is_synchronizable — a captureless lambda is empty, nothing to "
              "race on");
static_assert(!is_synchronizable_v<const decltype([x = 42] {})>,
              "is_synchronizable — a closure reflects no members, so its "
              "captures are state the recursion cannot inspect");
static_assert(is_synchronizable_v<std::atomic<int>[4]> && is_synchronizable_v<const int[4]>,
              "is_synchronizable — owned storage follows its element type");
static_assert(!is_synchronizable_v<const Node> && !is_synchronizable_v<const PList>,
              "is_synchronizable — a self-pointer asks the full trait of its "
              "pointee, so the recursion terminates instead of chasing itself");
static_assert(is_synchronizable_v<const ImmutableNode>,
              "is_synchronizable — the macro on a const type asserts read-only "
              "sharing; the full specialization outranks the partial one");
