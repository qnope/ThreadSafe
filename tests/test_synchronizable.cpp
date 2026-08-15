#include <threadsafe/synchronizable.h>

#include <atomic>

namespace {
struct Plain {};
struct SyncType {};
}

template <>
constexpr bool threadsafe::is_synchronizable<SyncType> = true;

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
