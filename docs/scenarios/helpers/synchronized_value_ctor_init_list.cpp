#include <threadsafe/threadsafe.h>
#include <vector>
int main() {
    threadsafe::synchronized_value<std::vector<int>> from_init_list{{1, 2, 3}};
    (void)from_init_list;
}
