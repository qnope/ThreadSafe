#include <threadsafe/threadsafe.h>
#include <map>
#include <string>
#include <vector>
struct Foo { int *borrowed; };
int main() {
    threadsafe::assert_sendable<std::map<std::string, std::vector<Foo>>>();
}
