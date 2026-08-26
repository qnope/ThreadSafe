#include <threadsafe/threadsafe.h>
#include <string>
using threadsafe::copy_on_write;
using threadsafe::is_synchronizable_v;
using threadsafe::synchronized_value;

// (a) the trait now answers instead of hard-erroring
static_assert(!is_synchronizable_v<const synchronized_value<int*>>);
static_assert(is_synchronizable_v<const synchronized_value<int>>);

// (b) a copy_on_write read through const is what the type is for
static_assert(is_synchronizable_v<const copy_on_write<std::string>>);
static_assert(!is_synchronizable_v<const copy_on_write<int*>>);
struct HoldsCow { copy_on_write<std::string> text; };
static_assert(is_synchronizable_v<const HoldsCow>);
int main() {}
