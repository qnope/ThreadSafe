#include <threadsafe/threadsafe.h>
struct Plain { int value; };
static_assert(threadsafe::is_synchronizable_v<const Plain>);
