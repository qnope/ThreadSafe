#include <threadsafe/threadsafe.h>
#include <cstdio>
#include <vector>

int main() {
    threadsafe::synchronized_value<std::vector<int>> shared_vector{};

    shared_vector.lock();                 // lock taken and released: no-op
    shared_vector.lock_shared();          // idem
    (void)shared_vector.lock();           // silences even the warning
    static_cast<void>(shared_vector.lock_shared());

    threadsafe::synchronized_value<int>::make(1);   // the shared_ptr is dropped

    std::printf("built and ran\n");
    return 0;
}
