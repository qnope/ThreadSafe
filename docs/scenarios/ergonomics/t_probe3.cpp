#include <meta>
#include <print>
#include <string>
// A constructor template whose *body* is invalid for U = the class itself,
// but whose declaration substitutes fine.
struct Bad { int x = 0; Bad() = default; template <class U> Bad(U&&) { static_assert(sizeof(U) == 999, "body"); } };
consteval bool probe() {
    using namespace std::meta;
    for (info m : members_of(^^Bad, access_context::unchecked()))
        if (is_constructor_template(m)) return can_substitute(m, {^^Bad});
    return false;
}
int main(){ std::print("can_substitute on a ctor with an invalid body: {}\n", probe()); }
