#include <threadsafe/threadsafe.h>
struct Flat { int member_0; };
static_assert(threadsafe::is_lifetime_aware_v<Flat>);
