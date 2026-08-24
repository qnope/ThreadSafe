#include <threadsafe/threadsafe.h>
#include <atomic>
namespace { struct Atomic { std::atomic<int> v; }; }
template <> struct threadsafe::is_synchronizable<Atomic> : std::true_type {};
using threadsafe::is_lifetime_aware_v;
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;

// sendable and lifetime_aware strip cv from the element; synchronizable does not.
static_assert(is_sendable_v<volatile int[3]>);
static_assert(is_lifetime_aware_v<volatile int[3]>);
static_assert(is_synchronizable_v<Atomic[3]>,      "plain array of a vouched element: yes");
static_assert(!is_synchronizable_v<volatile Atomic[3]>,
              "but volatile-qualifying the element flips it -- the other two traits do not");
// the documented volatile rule is unaffected either way
static_assert(!is_synchronizable_v<volatile int> && is_synchronizable_v<const volatile int>);
