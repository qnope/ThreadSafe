#include <threadsafe/threadsafe.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {
struct SyncType {};
struct BadDeleter {
    BadDeleter(const BadDeleter&);
    void operator()(int*) const;
};
struct Tree {
    std::unique_ptr<Tree> left;
};
struct SharedNode {
    std::shared_ptr<SharedNode> next;
};
struct ImmutableInt {
    const int value;
};
}

template <>
struct threadsafe::is_synchronizable<SyncType> : std::true_type {};

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

static_assert(is_sendable_v<std::unique_ptr<int>>,
              "is_sendable — the default deleter is stateless, so only the pointee matters");
static_assert(is_sendable_v<std::unique_ptr<std::unique_ptr<int>>>,
              "is_sendable — nested unique_ptr recurses through the pointee rule");
static_assert(is_sendable_v<std::unique_ptr<int[]>>,
              "is_sendable — the array form follows the element type");
static_assert(is_sendable_v<std::unique_ptr<const int>>,
              "is_sendable — cv on the pointee forwards to the unqualified type");
static_assert(is_sendable_v<std::unique_ptr<int, void (*)(int*)>>,
              "is_sendable — a function-pointer deleter shares code, not data");
static_assert(!is_sendable_v<std::unique_ptr<int*>>,
              "is_sendable — a non-sendable pointee blocks the unique_ptr");
static_assert(!is_sendable_v<std::unique_ptr<int, BadDeleter>>,
              "is_sendable — the deleter travels with the pointer, so it must be sendable");

static_assert(!is_sendable_v<std::shared_ptr<int>>,
              "is_sendable — sending a shared_ptr shares a non-synchronizable referent");
static_assert(!is_sendable_v<std::shared_ptr<void>>,
              "is_sendable — shared_ptr<void> shares an unknowable referent");
static_assert(!is_synchronizable_v<std::shared_ptr<void>>
                  && !is_synchronizable_v<const std::shared_ptr<void>>
                  && !is_lifetime_aware_v<std::shared_ptr<void>>,
              "shared_ptr<void> erases the referent: no trait may be granted on "
              "a static type that says nothing about the object held");
static_assert(!is_sendable_v<std::weak_ptr<void>>
                  && !is_synchronizable_v<std::weak_ptr<void>>
                  && !is_synchronizable_v<const std::weak_ptr<void>>
                  && !is_lifetime_aware_v<std::weak_ptr<void>>,
              "the weak_ptr form erases the referent just the same");
static_assert(!is_sendable_v<std::weak_ptr<int>>,
              "is_sendable — a weak_ptr can be locked into shared access");
static_assert(is_sendable_v<std::shared_ptr<SyncType>>,
              "is_sendable — a synchronizable referent may be shared across threads");
static_assert(is_sendable_v<std::shared_ptr<const SyncType>>,
              "is_sendable — cv on the referent is stripped, like the T& rule");
static_assert(!is_sendable_v<std::shared_ptr<const int>>,
              "is_sendable — cv on the referent is stripped, so int is asked, "
              "not const int: another handle may be a shared_ptr<int>");
static_assert(is_synchronizable_v<const ImmutableInt> && !is_synchronizable_v<ImmutableInt>,
              "is_synchronizable — a const-member struct is read-safe through "
              "const, which says nothing about the unqualified type");
static_assert(!is_sendable_v<std::shared_ptr<ImmutableInt>>,
              "is_sendable — burying the const in a member changes nothing: "
              "the rule asks the full trait of the pointee, never <const T>");
static_assert(is_sendable_v<std::shared_ptr<SyncType[]>>,
              "is_sendable — the array form follows the element type");
static_assert(is_sendable_v<std::weak_ptr<SyncType>>,
              "is_sendable — locking a sent weak_ptr yields shared access, which is safe here");

static_assert(!is_sendable_v<std::reference_wrapper<int>>,
              "is_sendable — a reference_wrapper shares its referent like T&");
static_assert(is_sendable_v<std::reference_wrapper<SyncType>>,
              "is_sendable — a reference_wrapper to a synchronizable type may travel");
static_assert(is_sendable_v<std::reference_wrapper<const SyncType>>,
              "is_sendable — cv on the referent is stripped, like the T& rule");

static_assert(is_sendable_v<const std::shared_ptr<SyncType>>,
              "is_sendable — cv on the wrapper forwards to the specialization");
static_assert(!is_sendable_v<std::shared_ptr<SyncType>&>,
              "is_sendable — the T& rule wins: the shared_ptr object itself is not synchronizable");

static_assert(is_sendable_v<std::atomic<std::shared_ptr<SyncType>>>,
              "is_sendable — an atomic follows the sendability of its value type");
static_assert(threadsafe::is_synchronizable_v<std::atomic<std::shared_ptr<SyncType>>>,
              "is_synchronizable — atomic of a sendable shared_ptr is synchronizable");
static_assert(!is_sendable_v<std::atomic<std::shared_ptr<int>>>,
              "is_sendable — an atomic shared_ptr still shares its referent when sent");
static_assert(!threadsafe::is_synchronizable_v<std::atomic<std::shared_ptr<int>>>,
              "is_synchronizable — atomic<T> is synchronizable only when T is sendable");
static_assert(is_sendable_v<std::vector<std::unique_ptr<int>>>,
              "is_sendable — containers of smart pointers compose");

static_assert(!is_synchronizable_v<const std::unique_ptr<int>>,
              "is_synchronizable — get() const hands out a plain int*");
static_assert(is_synchronizable_v<const std::unique_ptr<const int>>,
              "is_synchronizable — owned storage: the element keeps its own cv "
              "through get(), on the same two assumptions the sendable rule "
              "makes — no other alias, and a known dynamic type");
static_assert(!is_synchronizable_v<const std::unique_ptr<const int, BadDeleter>>,
              "is_synchronizable — the deleter is stored, so it is read too");
static_assert(is_synchronizable_v<const std::default_delete<int>>,
              "is_synchronizable — same explicit rule as is_sendable, for the "
              "same constructor-template reason");
static_assert(!is_synchronizable_v<const std::shared_ptr<std::string>>
                  && !is_synchronizable_v<const std::shared_ptr<const std::string>>,
              "is_synchronizable — another handle to the same object may be a "
              "shared_ptr<T>: the pointee's const is never trusted");
static_assert(is_synchronizable_v<const std::shared_ptr<std::atomic<int>>>,
              "is_synchronizable — copying the const handle is refcount-atomic-"
              "safe, and this pointee handles its own concurrency");
static_assert(!is_synchronizable_v<const std::weak_ptr<const std::string>>
                  && is_synchronizable_v<const std::weak_ptr<std::atomic<int>>>,
              "is_synchronizable — a weak_ptr locks into the same access as "
              "shared_ptr");
static_assert(!is_synchronizable_v<const std::reference_wrapper<int>>
                  && !is_synchronizable_v<const std::reference_wrapper<const int>>,
              "is_synchronizable — a const wrapper still shares its referent, "
              "whose own const proves nothing about other aliases");
static_assert(is_synchronizable_v<const std::reference_wrapper<std::atomic<int>>>,
              "is_synchronizable — the referent handles its own concurrency");
static_assert(!is_synchronizable_v<const Tree> && !is_synchronizable_v<const SharedNode>,
              "is_synchronizable — an owning self-pointer asks the full trait "
              "of its pointee, which terminates the recursion");
