#include <threadsafe/threadsafe.h>
int main() {
    threadsafe::synchronized_value<int> sv{0};
    *sv.lock() = 5;
}
