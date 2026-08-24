#include <threadsafe/threadsafe.h>
#include <atomic>

struct Stats { std::atomic<long> hits; std::atomic<long> misses; };
struct Rotted { std::atomic<long> hits; long backlog; };
struct Base { std::atomic<int> n; };
struct Derived : Base { std::atomic<int> m; };
struct RefsAtomic { std::atomic<int>& a; };
struct HasCow { threadsafe::synchronized_value<int> v; };

THREADSAFE_SYNCHRONIZABLE_MEMBERS(Stats);
THREADSAFE_SYNCHRONIZABLE_MEMBERS(Rotted);
THREADSAFE_SYNCHRONIZABLE_MEMBERS(Base);
THREADSAFE_SYNCHRONIZABLE_MEMBERS(Derived);
THREADSAFE_SYNCHRONIZABLE_MEMBERS(RefsAtomic);
THREADSAFE_SYNCHRONIZABLE_MEMBERS(HasCow);

using threadsafe::is_synchronizable_v;
static_assert(is_synchronizable_v<Stats>);
static_assert(!is_synchronizable_v<Rotted>, "a plain member added later flips it back");
static_assert(is_synchronizable_v<Derived>);
static_assert(is_synchronizable_v<RefsAtomic>);
static_assert(is_synchronizable_v<HasCow>);
static_assert(is_synchronizable_v<Stats[4]>);
static_assert(threadsafe::is_sendable_v<std::shared_ptr<Stats>>);
static_assert(!threadsafe::is_sendable_v<std::shared_ptr<Rotted>>);
