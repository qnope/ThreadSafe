#include <threadsafe/threadsafe.h>
#include <mutex>
using sync_int = threadsafe::synchronized_value<int>;
int main() {
    sync_int left{0};
    sync_int right{0};
    std::scoped_lock both(left.mutex_, right.mutex_);
}
