#include <threadsafe/threadsafe.h>
#include <string>
#include <vector>
using threadsafe::is_sendable_v;
using threadsafe::is_synchronizable_v;
// The library's general rule: is_sendable<T&> strips cv and asks the FULL trait.
static_assert(is_synchronizable_v<const std::string>);
static_assert(!is_synchronizable_v<std::string>);
static_assert(!is_sendable_v<const std::string&>,
              "so `const std::string&` is rejected library-wide, not just for cow");
int main() {}
