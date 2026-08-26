#include <threadsafe/threadsafe.h>

int main() {
    threadsafe::assert_sendable<threadsafe::synchronized_value<int>::guard>();
}
