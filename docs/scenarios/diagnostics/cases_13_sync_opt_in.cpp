#include <threadsafe/threadsafe.h>

struct Plain { int value; };

int main() {
    threadsafe::assert_synchronizable<Plain>();
}
