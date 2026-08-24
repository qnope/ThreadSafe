#include <threadsafe/threadsafe.h>
#include <map>
#include <string>
#include <vector>
int main() {
    threadsafe::copy_on_write<std::vector<int>> numbers{{1, 2, 3}};
}
