#include <threadsafe/threadsafe.h>
struct Flat { int member_0; };
static_assert(threadsafe::is_synchronizable_v<const Flat>);
