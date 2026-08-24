#include <meta>
#include <cstdio>
consteval bool probe() {
    try {
        throw std::meta::exception(u8"x", ^^int);
    } catch (const std::meta::exception &e) {
        return e.from() == ^^int;
    }
}
static_assert(probe());
int main() {}
