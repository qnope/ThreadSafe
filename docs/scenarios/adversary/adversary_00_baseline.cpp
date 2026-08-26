#include <threadsafe/threadsafe.h>

struct Plain { int a; double b; };

static_assert(threadsafe::is_sendable_v<Plain>);
static_assert(threadsafe::is_synchronizable_v<const Plain>);
static_assert(!threadsafe::is_synchronizable_v<Plain>);
