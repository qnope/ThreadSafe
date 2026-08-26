#include <threadsafe/threadsafe.h>

struct NeverDefined;

int main() {
    threadsafe::assert_sendable<NeverDefined>();
}
