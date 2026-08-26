#include <threadsafe/threadsafe.h>
using threadsafe::is_lifetime_aware_v;
static_assert(is_lifetime_aware_v<void(&)()>, "GOT-FALSE fnref");
static_assert(is_lifetime_aware_v<void(*)()>, "GOT-FALSE fnptr");
static_assert(!is_lifetime_aware_v<int&>, "GOT-TRUE int&");
static_assert(!is_lifetime_aware_v<int*>, "GOT-TRUE int*");
static_assert(!is_lifetime_aware_v<int&&>, "GOT-TRUE int&&");
int main(){}
