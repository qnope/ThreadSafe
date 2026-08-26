#include <threadsafe/threadsafe.h>
#include <ranges>
#include <vector>
#include <string>
#include <meta>
#include <string_view>

using Vec = std::vector<int>;
inline bool positive(int x) { return x > 0; }
using FilterOverOwning = decltype(std::views::all(Vec{}) | std::views::filter(&positive));

consteval std::u8string dump(std::meta::info type, int depth = 0) {
    std::u8string out;
    if (depth > 3) return out;
    for (auto m : std::meta::nonstatic_data_members_of(type, std::meta::access_context::unchecked())) {
        for (int i = 0; i < depth; ++i) out += u8"  ";
        out += (std::meta::has_identifier(m) ? std::u8string(std::meta::u8identifier_of(m)) : std::u8string(u8"<anon>"));
        out += u8" : ";
        out += std::u8string(std::meta::u8display_string_of(std::meta::type_of(m)));
        out += u8" [aware=";
        out += threadsafe::is_lifetime_aware_type(std::meta::remove_cv(std::meta::type_of(m))) ? u8"1" : u8"0";
        out += u8"]\n";
        auto t = std::meta::remove_cv(std::meta::type_of(m));
        if (std::meta::is_class_type(t) || std::meta::is_union_type(t))
            out += dump(t, depth + 1);
    }
    for (auto b : std::meta::bases_of(type, std::meta::access_context::unchecked())) {
        for (int i = 0; i < depth; ++i) out += u8"  ";
        out += u8"(base) ";
        out += std::u8string(std::meta::u8display_string_of(std::meta::type_of(b)));
        out += u8"\n";
        out += dump(std::meta::type_of(b), depth + 1);
    }
    return out;
}
consteval void go() { throw std::meta::exception(dump(^^FilterOverOwning), ^^FilterOverOwning); }
static_assert((go(), true));
int main(){}
