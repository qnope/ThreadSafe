// Does GCC 16 support C++26 expansion statements?  If so, a template-based
// diagnose<T>() could write is_sendable_v<[:type_of(member):]> directly and
// drop trait_value/substitute entirely.
#include <meta>
#include <type_traits>

struct Point { int x; double y; };

template <class T>
consteval int count_scalar_members() {
    int scalars = 0;
    template for (constexpr std::meta::info member :
                  std::meta::nonstatic_data_members_of(
                      ^^T, std::meta::access_context::unchecked())) {
        if (std::is_scalar_v<[:std::meta::type_of(member):]>)
            ++scalars;
    }
    return scalars;
}

static_assert(count_scalar_members<Point>() == 2);
int main() {}
