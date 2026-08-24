#include <threadsafe/threadsafe.h>
#include <map>
#include <string>
#include <vector>

int main() {
    // Both of these are the natural spellings and both are rejected.
    threadsafe::copy_on_write<std::vector<int>> numbers{{1, 2, 3}};
    threadsafe::copy_on_write<std::map<int, std::string>> table{
        {{1, "a"}, {2, "b"}}};
    return numbers->size() + table.operator->()->size();
}
