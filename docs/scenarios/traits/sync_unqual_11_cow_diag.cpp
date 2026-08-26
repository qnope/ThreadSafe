#include <threadsafe/threadsafe.h>
#include <string>
static_assert((threadsafe::assert_synchronizable<const threadsafe::copy_on_write<std::string>>(), true));
int main() {}
