#include <threadsafe/threadsafe.h>
#include <string>
#include <vector>
using sync_vec = threadsafe::synchronized_value<std::vector<int>>;
void helper(sync_vec::guard& locked) { locked->push_back(7); }
void reader(sync_vec::const_guard& locked) { (void)locked->size(); }
int main() {
    auto shared = sync_vec::make();
    { auto guard = shared->lock(); guard->push_back(1); *guard = {1,2,3}; helper(guard); }
    { auto guard = shared->lock_shared(); (void)guard->size(); reader(guard); }
    const auto& const_ref = *shared;
    { auto guard = const_ref.lock_shared(); (void)(*guard).size(); }
    return 0;
}
