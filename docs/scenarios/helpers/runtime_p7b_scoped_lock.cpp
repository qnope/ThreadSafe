// What happens when the user, unable to reach the mutexes, tries the obvious
// thing: hand the two synchronized_values themselves to std::scoped_lock.
#include <threadsafe/threadsafe.h>
#include <mutex>
using account = threadsafe::synchronized_value<long long>;
int main() {
    account first{0}, second{0};
    std::scoped_lock both{first, second};
}
