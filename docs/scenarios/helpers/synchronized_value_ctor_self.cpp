#include <threadsafe/threadsafe.h>
int main() {
    threadsafe::synchronized_value<int> a{1};
    threadsafe::synchronized_value<int> b{a};   // deleted copy ctor?
    (void)b;
}
