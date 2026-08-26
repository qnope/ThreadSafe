#include <threadsafe/threadsafe.h>
#include <atomic>
#include <memory>
#include <vector>
#include <string>

namespace {
struct OptedOut {};
struct UserCopy { UserCopy(const UserCopy&); };
}
template <> struct threadsafe::is_sendable<std::vector<OptedOut>> : std::false_type {};

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

// unqualified has a specialization, the cv spelling must agree
static_assert(!is_sendable_v<std::vector<OptedOut>>);
static_assert(!is_sendable_v<const std::vector<OptedOut>>);
static_assert(!is_sendable_v<volatile std::vector<OptedOut>>);
static_assert(!is_sendable_v<const volatile std::vector<OptedOut>>);

static_assert(is_sendable_v<std::vector<int>>);
static_assert(is_sendable_v<const std::vector<int>>);
static_assert(is_sendable_v<volatile std::vector<int>>);

static_assert(is_sendable_v<std::atomic<int>>);
static_assert(is_sendable_v<volatile std::atomic<int>>);
static_assert(is_sendable_v<const std::atomic<int>>);

static_assert(is_sendable_v<std::unique_ptr<int>>);
static_assert(is_sendable_v<const std::unique_ptr<int>>);
static_assert(is_sendable_v<volatile std::unique_ptr<int>>);

static_assert(!is_sendable_v<UserCopy>);
static_assert(!is_sendable_v<const UserCopy>);
static_assert(!is_sendable_v<volatile UserCopy>);

static_assert(is_sendable_v<std::string>);
static_assert(is_sendable_v<const std::string>);

// int* const volatile handled like int*
static_assert(!is_sendable_v<int*>);
static_assert(!is_sendable_v<int* const volatile>);
