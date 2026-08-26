#include <threadsafe/threadsafe.h>

#include <atomic>
#include <barrier>
#include <bitset>
#include <chrono>
#include <complex>
#include <condition_variable>
#include <latch>
#include <mutex>
#include <ratio>
#include <semaphore>
#include <shared_mutex>
#include <system_error>
#include <thread>
#include <typeindex>

using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
using threadsafe::is_lifetime_aware_v;

#define P(NAME, ...) static_assert(__VA_ARGS__, "PROBE " NAME)

P("SYNC atomic<int>",        is_synchronizable_v<std::atomic<int>>);
P("SYNC atomic_flag",        is_synchronizable_v<std::atomic_flag>);
P("SYNC atomic_ref<int>",    is_synchronizable_v<std::atomic_ref<int>>);
P("SYNC latch",              is_synchronizable_v<std::latch>);
P("SYNC binary_semaphore",   is_synchronizable_v<std::binary_semaphore>);
P("SYNC mutex",              is_synchronizable_v<std::mutex>);
P("SYNC shared_mutex",       is_synchronizable_v<std::shared_mutex>);
P("SYNC condition_variable", is_synchronizable_v<std::condition_variable>);
P("CSYNC const atomic<int>", is_synchronizable_v<const std::atomic<int>>);
P("CSYNC const atomic_flag", is_synchronizable_v<const std::atomic_flag>);

P("S atomic_flag",           is_sendable_v<std::atomic_flag>);
P("S atomic_ref<int>",       is_sendable_v<std::atomic_ref<int>>);
P("S latch",                 is_sendable_v<std::latch>);
P("S mutex",                 is_sendable_v<std::mutex>);
P("S shared_mutex",          is_sendable_v<std::shared_mutex>);
P("S atomic_flag&",          is_sendable_v<std::atomic_flag&>);
P("S atomic<int>&",          is_sendable_v<std::atomic<int>&>);
P("S latch&",                is_sendable_v<std::latch&>);

P("S thread::id",            is_sendable_v<std::thread::id>);
P("S error_code",            is_sendable_v<std::error_code>);
P("S type_index",            is_sendable_v<std::type_index>);
P("S ratio<1,2>",            is_sendable_v<std::ratio<1,2>>);
P("S chrono::seconds",       is_sendable_v<std::chrono::seconds>);
P("S steady_clock::time_point", is_sendable_v<std::chrono::steady_clock::time_point>);
P("S complex<float>",        is_sendable_v<std::complex<float>>);
P("S bitset<64>",            is_sendable_v<std::bitset<64>>);

int main() {}
