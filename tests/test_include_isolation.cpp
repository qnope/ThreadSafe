#include <threadsafe/lifetime_aware.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

using threadsafe::is_lifetime_aware;
using threadsafe::is_sendable;
using threadsafe::is_synchronizable;

static_assert(is_lifetime_aware<std::vector<int>>);
static_assert(is_lifetime_aware<std::string>);
static_assert(is_sendable<std::vector<int>>);
static_assert(is_sendable<std::string>);

static_assert(is_sendable<std::unique_ptr<int>>);

static_assert(is_synchronizable<std::atomic<int>>);
static_assert(is_sendable<std::atomic<int>&>,
              "sending a reference to an atomic shares a synchronizable object");
static_assert(is_sendable<void (*)()>,
              "function pointers rely on function types being synchronizable");
