#include <threadsafe/threadsafe.h>
#include <functional>

int main() { threadsafe::assert_lifetime_aware<std::reference_wrapper<int>>(); }
