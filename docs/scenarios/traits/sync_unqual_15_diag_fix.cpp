#include <threadsafe/threadsafe.h>
#include <optional>
#include <string>
#include <vector>
static_assert((threadsafe::assert_synchronizable<const std::vector<int*>>(), true));
int main() {}
