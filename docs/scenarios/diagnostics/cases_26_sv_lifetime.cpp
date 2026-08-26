#include <threadsafe/threadsafe.h>
#include <string_view>
int main() { threadsafe::assert_lifetime_aware<std::string_view>(); }
