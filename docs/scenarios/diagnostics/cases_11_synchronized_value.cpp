#include <threadsafe/threadsafe.h>

struct Borrowing { int *pointer; };

int main() {
    threadsafe::synchronized_value<Borrowing> value{};
    (void)value;
}
