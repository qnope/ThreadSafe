#include <threadsafe/threadsafe.h>

struct Borrowing {
    int *borrowed;
};

int main() {
    threadsafe::synchronized_value<Borrowing> guarded;
    (void) guarded;
}
