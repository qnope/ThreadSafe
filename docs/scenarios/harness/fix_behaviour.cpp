#include <threadsafe/threadsafe.h>
#include <atomic>
#include <memory>

struct Stats { std::atomic<long> hits; std::atomic<long> misses; };
struct Derived : Stats {};
struct RefsAtomic { std::atomic<int>& a; };
struct HasCow { threadsafe::synchronized_value<int> v; };
struct Rotted { std::atomic<long> hits; long backlog; };
struct HasStatic { std::atomic<long> hits; static long shared_counter; };

THREADSAFE_SYNCHRONIZABLE_MEMBERS(Stats);
THREADSAFE_SYNCHRONIZABLE_MEMBERS(Derived);
THREADSAFE_SYNCHRONIZABLE_MEMBERS(RefsAtomic);
THREADSAFE_SYNCHRONIZABLE_MEMBERS(HasCow);
THREADSAFE_SYNCHRONIZABLE_MEMBERS(Rotted);
THREADSAFE_SYNCHRONIZABLE_MEMBERS(HasStatic);

using threadsafe::is_synchronizable_v;
static_assert(is_synchronizable_v<Stats>);
static_assert(is_synchronizable_v<Stats[4]>);
static_assert(threadsafe::is_sendable_v<std::shared_ptr<Stats>>);
static_assert(is_synchronizable_v<Derived>);
static_assert(is_synchronizable_v<RefsAtomic>);
static_assert(is_synchronizable_v<HasCow>);
static_assert(!is_synchronizable_v<Rotted>);      // rot IS caught
static_assert(is_synchronizable_v<HasStatic>);    // static state NOT walked
int main() {}
