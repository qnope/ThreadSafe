// sendable and lifetime_aware exist as concepts. Does synchronizable?
#include <threadsafe/threadsafe.h>

#include <atomic>

template <threadsafe::sendable T>
void send(T) {}

template <threadsafe::lifetime_aware T>
void own(T) {}

template <threadsafe::synchronizable T>
void share(T &) {}

int main() {
    std::atomic<int> counter{0};
    share(counter);
}
