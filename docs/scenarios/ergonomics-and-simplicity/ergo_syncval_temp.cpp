#include <threadsafe/threadsafe.h>
#include <vector>
int main() {
    threadsafe::synchronized_value<std::vector<int>> values;
    values.lock()->push_back(1);
}
