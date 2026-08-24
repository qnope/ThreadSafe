#include <threadsafe/threadsafe.h>
#include <memory>

struct implementation;

static_assert(!threadsafe::is_sendable_v<std::unique_ptr<implementation>>);

int main() {}
