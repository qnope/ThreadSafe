#include <threadsafe/threadsafe.h>
#include <string>

int main() {
    threadsafe::synchronized_value<int> value{std::string{"not an int"}};
    (void)value;
}
