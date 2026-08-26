// Every escape the deleted rvalue operators do NOT close.  Compiles clean at
// -Wall -Wextra with g++-16; each line hands a reference or a pointer to the
// protected object out from under the lock.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <functional>
#include <thread>
#include <vector>

using sync_vector = threadsafe::synchronized_value<std::vector<int>>;

// Hole 1 -- bind an lvalue reference through the lvalue operator*, then let the
// guard die. The reference outlives the lock.
int& hole_reference_outlives_guard(sync_vector& shared_vector) {
    const auto guard = shared_vector.lock();
    int& first = (*guard)[0];
    return first;                       // lock released on return
}

// Hole 2 -- store the T* itself.
std::vector<int>* hole_pointer_escapes(sync_vector& shared_vector) {
    const auto guard = shared_vector.lock();
    return guard.operator->();
}

// Hole 3 -- capture *guard in a lambda that escapes the scope.
std::function<void()> hole_lambda_escapes(sync_vector& shared_vector) {
    const auto guard = shared_vector.lock();
    std::vector<int>& value = *guard;
    return [&value] { value.push_back(1); };   // runs unlocked, later
}

// Hole 4 -- the reference survives the enclosing scope of the guard.
void hole_scope(sync_vector& shared_vector) {
    std::vector<int>* leaked = nullptr;
    {
        const auto guard = shared_vector.lock();
        leaked = &*guard;
    }                                    // unlocked here
    leaked->push_back(2);                // unsynchronised write
}

int main() {
    sync_vector shared_vector{std::vector<int>{0}};

    int& escaped_reference = hole_reference_outlives_guard(shared_vector);
    std::vector<int>* escaped_pointer = hole_pointer_escapes(shared_vector);
    std::function<void()> escaped_lambda = hole_lambda_escapes(shared_vector);

    std::jthread other{[&] {
        for (int i = 0; i < 1000; ++i) {
            const auto guard = shared_vector.lock();
            guard->push_back(i);
        }
    }};

    escaped_reference = 42;              // unsynchronised write, no lock held
    escaped_pointer->push_back(3);       // unsynchronised write
    escaped_lambda();                    // unsynchronised write
    hole_scope(shared_vector);

    other.join();
    const auto final_guard = shared_vector.lock();
    std::printf("survived, size=%zu\n", final_guard->size());
    return 0;
}
