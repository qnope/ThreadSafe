// The guard is destroyed at the end of the full expression, so the lock is
// still held while push_back runs -- yet the one-liner is deleted.
#include <threadsafe/threadsafe.h>

#include <vector>

int main() {
    threadsafe::synchronized_value<std::vector<int>> pending_items{};
    pending_items.lock()->push_back(1);
}
