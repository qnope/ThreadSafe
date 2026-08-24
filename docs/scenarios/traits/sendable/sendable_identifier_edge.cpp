#include <meta>
#include <print>
#include <vector>

template <class T> struct Tmpl { int v; };
namespace { struct InAnonNamespace { int v; }; }

consteval bool local_class_has_identifier() {
    struct Local { int v; };
    return std::meta::has_identifier(^^Local);
}

int main() {
    std::println("vector<int>        has_identifier={}", std::meta::has_identifier(^^std::vector<int>));
    std::println("Tmpl<int>          has_identifier={}", std::meta::has_identifier(^^Tmpl<int>));
    std::println("InAnonNamespace    has_identifier={}", std::meta::has_identifier(^^InAnonNamespace));
    std::println("local class        has_identifier={}", local_class_has_identifier());
}
