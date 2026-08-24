#include <threadsafe/threadsafe.h>
#include <string>
consteval void explain() {
    threadsafe::assert_synchronizable<const threadsafe::copy_on_write<std::string>>();
}
static_assert((explain(), true));
int main() {}
