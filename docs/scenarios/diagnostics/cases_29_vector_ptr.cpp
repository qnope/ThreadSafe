#include <threadsafe/threadsafe.h>
#include <vector>
struct Foo { int value; };
int main() { threadsafe::assert_sendable<std::vector<Foo *>>(); }
