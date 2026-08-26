#include <threadsafe/threadsafe.h>
#include <string>
int main() {
    threadsafe::assert_synchronizable<const threadsafe::copy_on_write<std::string>>();
}
