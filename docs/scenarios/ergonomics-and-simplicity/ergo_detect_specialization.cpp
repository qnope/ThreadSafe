// Can a consteval helper tell "the trait answered from an explicit/partial
// specialization" from "the trait answered from the primary template"?
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace {
struct PlainAggregate {
    int value;
};

consteval const char* trait_source_file(std::meta::info type) {
    return std::meta::source_location_of(
               std::meta::substitute(^^threadsafe::is_sendable, {type}))
        .file_name();
}
consteval unsigned trait_source_line(std::meta::info type) {
    return std::meta::source_location_of(
               std::meta::substitute(^^threadsafe::is_sendable, {type}))
        .line();
}
}

#define SHOW(...)                                                              \
    std::printf("%-46s %s:%u\n", #__VA_ARGS__,                                 \
                trait_source_file(^^__VA_ARGS__),                              \
                trait_source_line(^^__VA_ARGS__))

int main() {
    SHOW(PlainAggregate);
    SHOW(std::shared_ptr<const std::map<std::string, std::string>>);
    SHOW(std::reference_wrapper<int>);
    SHOW(std::vector<int>);
}
