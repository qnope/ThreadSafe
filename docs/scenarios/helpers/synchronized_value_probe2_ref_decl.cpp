#include <threadsafe/threadsafe.h>
using sync_int = threadsafe::synchronized_value<int>;
using sync_ref = threadsafe::synchronized_value<sync_int&>;
static_assert(sizeof(sync_ref) > 0, "the class itself instantiates");
static_assert(!threadsafe::is_lifetime_aware_v<sync_ref>);
static_assert(threadsafe::is_synchronizable_v<sync_ref>);
int main() {
    sync_int owned{7};
    sync_ref borrower{owned};      // constructs, binding the reference member
    (void)borrower;
}
