#include <meta>
#include <type_traits>

struct Point { int x; double y; };

template <class T>
consteval int count_scalar_members() {
    int scalars = 0;
    template for (constexpr std::meta::info member :
                  std::define_static_array(
                      std::meta::nonstatic_data_members_of(
                          ^^T, std::meta::access_context::unchecked()))) {
        if (std::is_scalar_v<typename [:std::meta::type_of(member):]>)
            ++scalars;
    }
    return scalars;
}

static_assert(count_scalar_members<Point>() == 2);
int main() {}
