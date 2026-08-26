#include <threadsafe/threadsafe.h>
#include <bitset>
#include <chrono>
#include <complex>
#include <expected>
#include <flat_map>
#include <functional>
#include <latch>
#include <mutex>
#include <queue>
#include <span>
#include <stack>
#include <filesystem>

namespace { struct SyncType {}; }
THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(SyncType);

static_assert((threadsafe::assert_sendable<std::complex<double>>(), true));
static_assert((threadsafe::assert_sendable<std::chrono::milliseconds>(), true));
static_assert((threadsafe::assert_sendable<std::expected<int,int>>(), true));
static_assert((threadsafe::assert_sendable<std::stack<int>>(), true));
static_assert((threadsafe::assert_sendable<std::bitset<8>>(), true));
static_assert((threadsafe::assert_sendable<std::span<SyncType>>(), true));
static_assert((threadsafe::assert_sendable<std::flat_map<int,int>>(), true));
static_assert((threadsafe::assert_sendable<std::filesystem::path>(), true));
static_assert((threadsafe::assert_synchronizable<std::latch>(), true));
static_assert((threadsafe::assert_synchronizable<std::atomic_flag>(), true));
static_assert((threadsafe::assert_lifetime_aware<std::reference_wrapper<void()>>(), true));
int main() {}
