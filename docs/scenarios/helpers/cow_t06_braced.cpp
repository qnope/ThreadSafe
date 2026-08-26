#include <threadsafe/threadsafe.h>
#include <vector>
int main() {
    threadsafe::copy_on_write<std::vector<int>> elements{1, 2, 3};
}
