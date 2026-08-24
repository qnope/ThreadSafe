#include <threadsafe/threadsafe.h>
#include <meta>
#include <cstdio>

namespace {
struct AssignTemplate {          // CAN hijack a move assignment
    int value;
    template <class U> AssignTemplate &operator=(U &&);
};
struct EqualityTemplate {        // CANNOT hijack anything: it is a comparison
    int value;
    template <class U> bool operator==(const U &) const;
};
struct DefaultedSpaceship {
    int value;
    auto operator<=>(const DefaultedSpaceship &) const = default;
};
struct CallOperatorTemplate {
    int value;
    template <class U> void operator()(U &&) const;
};
}

int main() {
    std::printf("op_equals == operator= ? %d ; == operator== ? %d\n",
        (int)[] consteval {
            for (std::meta::info member : std::meta::members_of(
                     ^^AssignTemplate, std::meta::access_context::unchecked()))
                if (std::meta::is_operator_function_template(member))
                    return std::meta::operator_of(member) == std::meta::op_equals;
            return false;
        }(),
        (int)[] consteval {
            for (std::meta::info member : std::meta::members_of(
                     ^^EqualityTemplate, std::meta::access_context::unchecked()))
                if (std::meta::is_operator_function_template(member))
                    return std::meta::operator_of(member) == std::meta::op_equals;
            return false;
        }());
    std::printf("AssignTemplate     const-sync=%d (want 0)\n",
                (int)threadsafe::is_synchronizable_v<const AssignTemplate>);
    std::printf("EqualityTemplate   const-sync=%d (want 1)\n",
                (int)threadsafe::is_synchronizable_v<const EqualityTemplate>);
    std::printf("DefaultedSpaceship const-sync=%d (want 1)\n",
                (int)threadsafe::is_synchronizable_v<const DefaultedSpaceship>);
    std::printf("CallOpTemplate     const-sync=%d (want 1)\n",
                (int)threadsafe::is_synchronizable_v<const CallOperatorTemplate>);
    return 0;
}
