#include <meta>
#include <print>

struct ReservedRegister { unsigned : 32; };
struct NamedBitfield { unsigned bits : 8; };
struct MixedBitfields { unsigned : 3; unsigned ready : 1; unsigned : 28; };
union EmptyUnion {};
struct EmptyStruct {};
using Closure = decltype([x = 42, y = static_cast<int*>(nullptr)] {});

struct Facts {
    bool has_identifier;
    std::size_t members;
    std::size_t nsdm;
    bool empty;
    bool any_bitfield;
};

template <typename T>
consteval Facts facts() {
    using namespace std::meta;
    const auto ctx = access_context::unchecked();
    bool any_bitfield = false;
    for (info m : nonstatic_data_members_of(^^T, ctx))
        if (is_bit_field(m)) any_bitfield = true;
    return Facts{std::meta::has_identifier(^^T), members_of(^^T, ctx).size(),
                 nonstatic_data_members_of(^^T, ctx).size(), is_empty_type(^^T),
                 any_bitfield};
}

#define ROW(...) do { constexpr Facts f = facts<__VA_ARGS__>(); \
    std::println("{:<20} has_identifier={:<5} members={:<3} nsdm={:<3} empty={:<5} bitfield={}", \
                 #__VA_ARGS__, f.has_identifier, f.members, f.nsdm, f.empty, f.any_bitfield); } while(0)

int main() {
    ROW(ReservedRegister);
    ROW(NamedBitfield);
    ROW(MixedBitfields);
    ROW(EmptyUnion);
    ROW(EmptyStruct);
    ROW(Closure);
}
