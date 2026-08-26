#include <threadsafe/threadsafe.h>

struct Borrowing { int *pointer; };

template <threadsafe::sendable T>
void hand_over(T) {}

int main() { hand_over(Borrowing{}); }
