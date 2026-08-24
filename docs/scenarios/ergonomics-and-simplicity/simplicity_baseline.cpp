#include <threadsafe/threadsafe.h>
#include <string>
#include <vector>
static_assert(threadsafe::is_sendable_v<std::vector<std::string>>);
int main() {}
