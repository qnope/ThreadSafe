#include <threadsafe/threadsafe.h>
#include <vector>

struct Leaf { mutable int cached; };

int main() {
    threadsafe::assert_synchronizable<const std::vector<Leaf>>();
}
