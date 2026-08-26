#include <meta>
#include <print>
#include <string>
#include <vector>
#include <concepts>

struct Greedy { int x; template <class U> Greedy(U&&) {} };
struct IterPair { int x; template <class It> IterPair(It, It) {} };
struct Converting { int x; template <class U> requires (!std::same_as<std::remove_cvref_t<U>, Converting>) Converting(U&&) {} };
struct Variadic { int x; template <class... A> Variadic(A&&...) {} };

template <class T>
consteval const char* report() {
    using namespace std::meta;
    std::string out;
    for (info m : members_of(^^T, access_context::unchecked())) {
        if (!is_constructor_template(m)) continue;
        out += "\n   ctor tmpl " + std::string(display_string_of(m));
        out += " | can_sub{T&}=" + std::string(can_substitute(m, {^^T&}) ? "Y" : "N");
        out += " can_sub{T}="  + std::string(can_substitute(m, {^^T})  ? "Y" : "N");
        out += " can_sub{int}=" + std::string(can_substitute(m, {^^int}) ? "Y" : "N");
        if (can_substitute(m, {^^T&})) {
            info s = substitute(m, {^^T&});
            out += " -> " + std::string(display_string_of(s));
            auto ps = parameters_of(s);
            out += " nparams=" + std::string(1, char('0' + ps.size()));
            for (info p : ps) out += " [" + std::string(display_string_of(type_of(p))) + "]";
        }
    }
    return define_static_string(out);
}
template <class T> void show(const char* n) { std::print("== {}{}\n", n, report<T>()); }
int main() { show<Greedy>("Greedy"); show<IterPair>("IterPair"); show<Converting>("Converting"); show<Variadic>("Variadic"); }
