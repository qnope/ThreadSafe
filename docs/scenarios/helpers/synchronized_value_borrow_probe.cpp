#include <threadsafe/threadsafe.h>
#include <functional>
#include <memory>

using sync_int = threadsafe::synchronized_value<int>;
using sync_borrow = threadsafe::synchronized_value<sync_int*>;
using sync_refwrap = threadsafe::synchronized_value<std::reference_wrapper<sync_int>>;

using threadsafe::is_sendable_v;
using threadsafe::is_lifetime_aware_v;

// The entry guard is sendable<T> only, so a synchronized_value over a borrow
// compiles.
static_assert(is_sendable_v<sync_int*>);
static_assert(!is_lifetime_aware_v<sync_int*>);
static_assert(sizeof(sync_borrow) > 0);
static_assert(sizeof(sync_refwrap) > 0);

// ...but ownership is transitive through the wrapper, so nothing lifetime-aware
// can be built out of it, and the launcher refuses it.
static_assert(!is_lifetime_aware_v<sync_borrow>);
static_assert(!is_lifetime_aware_v<std::shared_ptr<sync_borrow>>);
static_assert(is_sendable_v<std::shared_ptr<sync_borrow>>,
              "sendable, yet not lifetime aware");
static_assert(!threadsafe::launchable_task<
                  decltype([](std::shared_ptr<sync_borrow>) {}),
                  std::shared_ptr<sync_borrow>>,
              "launch_task refuses it: the checked path is closed");
static_assert(threadsafe::launchable_scoped_task<
                  decltype([](std::shared_ptr<sync_borrow>) {}),
                  std::shared_ptr<sync_borrow>>,
              "launch_scoped_task accepts it, but it joins");
