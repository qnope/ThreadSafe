#include <threadsafe/threadsafe.h>
#include <memory>

struct Later;

// Asked while incomplete: silently false.
static_assert(!threadsafe::is_sendable_v<Later>);
static_assert(!threadsafe::is_synchronizable_v<const Later>);
static_assert(!threadsafe::is_lifetime_aware_v<Later>);

struct Later { int a; int b; };

// Asked again, now complete, in the SAME translation unit: the cached answer wins.
static_assert(!threadsafe::is_sendable_v<Later>, "STILL FALSE AFTER COMPLETION");
static_assert(!threadsafe::is_synchronizable_v<const Later>, "STILL FALSE");
static_assert(!threadsafe::is_lifetime_aware_v<Later>, "STILL FALSE");

// A fresh type completed before the first question answers the opposite.
struct Sooner { int a; int b; };
static_assert(threadsafe::is_sendable_v<Sooner>);
static_assert(threadsafe::is_synchronizable_v<const Sooner>);
static_assert(threadsafe::is_lifetime_aware_v<Sooner>);
