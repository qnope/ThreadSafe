#include <meta>
#include <print>
#include <string>
struct Fwd { int x = 0; Fwd() = default; template <class U> Fwd(U&&); };  // declaration only
struct FwdDef { int x = 0; FwdDef() = default; template <class U> FwdDef(U&& o) : x(o.x) {} };
struct Bad { int x = 0; Bad() = default; template <class U> Bad(U&&) { static_assert(sizeof(U) == 999); } };
consteval const char* try_parameters_of_template() {
    using namespace std::meta;
    std::string out;
    for (info m : members_of(^^Fwd, access_context::unchecked())) {
        if (!is_constructor_template(m)) continue;
        try { auto ps = parameters_of(m); out += "parameters_of(template) OK, n="; out += char('0'+ps.size()); }
        catch (std::meta::exception&) { out += "parameters_of(template) THREW"; }
        break;
    }
    return define_static_string(out);
}
consteval const char* subst_decl_only() {
    using namespace std::meta;
    std::string out;
    for (info m : members_of(^^Fwd, access_context::unchecked())) {
        if (!is_constructor_template(m)) continue;
        out += can_substitute(m, {^^Fwd}) ? "decl-only can_substitute{Fwd}=Y " : "decl-only can_substitute{Fwd}=N ";
        if (can_substitute(m, {^^Fwd}))
            for (info p : parameters_of(substitute(m, {^^Fwd}))) out += "[" + std::string(display_string_of(type_of(p))) + "]";
        break;
    }
    return define_static_string(out);
}
int main(){ std::print("{}\n{}\n", try_parameters_of_template(), subst_decl_only()); }
