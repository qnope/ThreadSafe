// With the proposed initializer_list constructor, copy_on_write is written
// exactly like the container it wraps.
#include <threadsafe/threadsafe.h>

#include <map>
#include <print>
#include <string>
#include <vector>

int main() {
    threadsafe::copy_on_write<std::vector<int>> numbers{1, 2, 3};
    threadsafe::copy_on_write<std::map<std::string, std::string>> settings{
        {"host", "localhost"}, {"port", "8080"}};

    std::println("numbers = {}, host = {}", numbers->size(),
                 settings->at("host"));
}
