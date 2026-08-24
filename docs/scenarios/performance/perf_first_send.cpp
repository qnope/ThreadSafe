#include <threadsafe/threadsafe.h>
struct Flat { int member_0; };
static_assert(threadsafe::is_sendable_v<Flat>);
