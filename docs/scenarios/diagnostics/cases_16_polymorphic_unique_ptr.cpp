#include <threadsafe/threadsafe.h>
#include <memory>

struct Base { virtual ~Base() = default; int value; };

int main() { threadsafe::assert_sendable<std::unique_ptr<Base>>(); }
