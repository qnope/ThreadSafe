#include <threadsafe/threadsafe.h>
#include <meta>
#include <string>

struct HasStatics {
    static inline long counter = 0;
    static constexpr int limit = 4;
    static thread_local inline int per_thread = 0;
    int ordinary = 0;
    void method() const {}
};

consteval std::size_t count_static_variables() {
    std::size_t found = 0;
    for (std::meta::info member :
         std::meta::members_of(^^HasStatics, std::meta::access_context::unchecked()))
        if (std::meta::is_variable(member))
            ++found;
    return found;
}

consteval std::u8string names_and_types() {
    std::u8string out;
    for (std::meta::info member :
         std::meta::members_of(^^HasStatics, std::meta::access_context::unchecked()))
        if (std::meta::is_variable(member))
            out += std::u8string(std::meta::u8identifier_of(member)) + u8":"
                 + std::u8string(std::meta::u8display_string_of(std::meta::type_of(member))) + u8" ";
    return out;
}

static_assert(count_static_variables() == 3, "is_variable sees the three statics");
static_assert(std::meta::nonstatic_data_members_of(^^HasStatics,
                  std::meta::access_context::unchecked()).size() == 1,
              "nonstatic_data_members_of sees only `ordinary`");

// print the names/types by forcing them into a diagnostic
static_assert(names_and_types().empty(), "NAMES");
