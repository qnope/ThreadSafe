#include <threadsafe/smart_pointers.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <threadsafe/containers.h>
#include <threadsafe/synchronizable.h>

namespace {
struct SyncType {};
struct BadDeleter {
    BadDeleter(const BadDeleter&);
    void operator()(int*) const;
};
}  // namespace

template <>
constexpr bool threadsafe::is_synchronizable<SyncType> = true;

using threadsafe::is_sendable;

// --- unique_ptr: pointee and deleter must both be sendable ---
static_assert(is_sendable<std::unique_ptr<int>>,
              "is_sendable — the default deleter is stateless, so only the pointee matters");
static_assert(is_sendable<std::unique_ptr<std::unique_ptr<int>>>,
              "is_sendable — nested unique_ptr recurses through the pointee rule");
static_assert(is_sendable<std::unique_ptr<int[]>>,
              "is_sendable — the array form follows the element type");
static_assert(is_sendable<std::unique_ptr<const int>>,
              "is_sendable — cv on the pointee forwards to the unqualified type");
static_assert(is_sendable<std::unique_ptr<int, void (*)(int*)>>,
              "is_sendable — a function-pointer deleter shares code, not data");
static_assert(!is_sendable<std::unique_ptr<int*>>,
              "is_sendable — a non-sendable pointee blocks the unique_ptr");
static_assert(!is_sendable<std::unique_ptr<int, BadDeleter>>,
              "is_sendable — the deleter travels with the pointer, so it must be sendable");

// --- shared_ptr / weak_ptr: sending shares the referent ---
static_assert(!is_sendable<std::shared_ptr<int>>,
              "is_sendable — sending a shared_ptr shares a non-synchronizable referent");
static_assert(!is_sendable<std::shared_ptr<void>>,
              "is_sendable — shared_ptr<void> shares an unknowable referent");
static_assert(!is_sendable<std::weak_ptr<int>>,
              "is_sendable — a weak_ptr can be locked into shared access");
static_assert(is_sendable<std::shared_ptr<SyncType>>,
              "is_sendable — a synchronizable referent may be shared across threads");
static_assert(is_sendable<std::shared_ptr<const SyncType>>,
              "is_sendable — cv on the referent is stripped, like the T& rule");
static_assert(is_sendable<std::shared_ptr<SyncType[]>>,
              "is_sendable — the array form follows the element type");
static_assert(is_sendable<std::weak_ptr<SyncType>>,
              "is_sendable — locking a sent weak_ptr yields shared access, which is safe here");

// --- reference_wrapper: same rule as T& ---
static_assert(!is_sendable<std::reference_wrapper<int>>,
              "is_sendable — a reference_wrapper shares its referent like T&");
static_assert(is_sendable<std::reference_wrapper<SyncType>>,
              "is_sendable — a reference_wrapper to a synchronizable type may travel");
static_assert(is_sendable<std::reference_wrapper<const SyncType>>,
              "is_sendable — cv on the referent is stripped, like the T& rule");

// --- cv and references ---
static_assert(is_sendable<const std::shared_ptr<SyncType>>,
              "is_sendable — cv on the wrapper forwards to the specialization");
static_assert(!is_sendable<std::shared_ptr<SyncType>&>,
              "is_sendable — the T& rule wins: the shared_ptr object itself is not synchronizable");

// --- interactions ---
static_assert(is_sendable<std::atomic<std::shared_ptr<SyncType>>>,
              "is_sendable — an atomic follows the sendability of its value type");
static_assert(threadsafe::is_synchronizable<std::atomic<std::shared_ptr<SyncType>>>,
              "is_synchronizable — atomic of a sendable shared_ptr is synchronizable");
static_assert(!is_sendable<std::atomic<std::shared_ptr<int>>>,
              "is_sendable — an atomic shared_ptr still shares its referent when sent");
static_assert(!threadsafe::is_synchronizable<std::atomic<std::shared_ptr<int>>>,
              "is_synchronizable — atomic<T> is synchronizable only when T is sendable");
static_assert(is_sendable<std::vector<std::unique_ptr<int>>>,
              "is_sendable — containers of smart pointers compose");
