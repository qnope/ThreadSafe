#include <threadsafe/threadsafe.h>
using namespace threadsafe;
struct Flat {
    int member_0;
};
static_assert(is_sendable_v<Flat>);
static_assert(is_synchronizable_v<const Flat>);
static_assert(is_lifetime_aware_v<Flat>);
