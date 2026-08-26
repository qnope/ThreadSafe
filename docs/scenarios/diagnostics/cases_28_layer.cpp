#include <meta>
#include <string>

namespace probe {
consteval std::u8string late_hook(std::meta::info type);

consteval std::u8string early_caller(std::meta::info type) {
    return late_hook(type);
}
}

// ... a later header would define it:
namespace probe {
consteval std::u8string late_hook(std::meta::info type) {
    return std::u8string(std::meta::u8display_string_of(type));
}
}

static_assert(probe::early_caller(^^int) == u8"int");
int main() {}
