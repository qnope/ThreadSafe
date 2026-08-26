#include <threadsafe/threadsafe.h>
#include <string>

int main() {
    threadsafe::copy_on_write<int> shared{std::string{"not an int"}};
    (void)shared;
}
