// The canonical one-liner on a mutex-protected value: take the lock, push, release.
#include <threadsafe/threadsafe.h>

#include <vector>

int main() {
    threadsafe::synchronized_value<std::vector<int>> shared_vector;
    shared_vector.lock()->push_back(42);
}
