#include <threadsafe/threadsafe.h>
using sync_int = threadsafe::synchronized_value<int>;
using sync_ref = threadsafe::synchronized_value<sync_int&>;
int main() {
    sync_int owned{7};
    sync_ref borrower{owned};
    auto guard = borrower.lock();
    (void)guard;
}
