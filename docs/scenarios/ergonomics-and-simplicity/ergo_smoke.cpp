#include <threadsafe/threadsafe.h>
static_assert(threadsafe::is_sendable_v<int>);
int main() {}
