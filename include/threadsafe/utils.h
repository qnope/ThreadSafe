#pragma once

#include <meta>
#include <type_traits>

namespace threadsafe::detail {

// A closure type reports no bases and no non-static data members, whatever it
// captures, so a recursion over the reflected members of one concludes "no
// state" for a lambda holding a dangling reference. Occupying storage while
// reflecting nothing is the signature of state the traits cannot inspect —
// except for a polymorphic class, whose unaccounted size is the vptr.
inline consteval bool has_unreflectable_state(std::meta::info type) {
    const auto ctx = std::meta::access_context::unchecked();
    return !std::meta::is_empty_type(type)
        && !std::meta::is_polymorphic_type(type)
        && std::meta::bases_of(type, ctx).empty()
        && std::meta::nonstatic_data_members_of(type, ctx).empty();
}

inline consteval bool is_copy_move_constructor_assignment(std::meta::info m) {
    return std::meta::is_copy_constructor(m)
        || std::meta::is_move_constructor(m)
        || std::meta::is_copy_assignment(m)
        || std::meta::is_move_assignment(m);
}

inline consteval bool
has_only_default_copy_move_constructor_assignment(std::meta::info type) {
    const auto ctx = std::meta::access_context::unchecked();
    for (std::meta::info m : std::meta::members_of(type, ctx)) {
        if (!is_copy_move_constructor_assignment(m))
            continue;

        if (!std::meta::is_defaulted(m) && !std::meta::is_deleted(m))
            return false;
    }
    return true;
}

}
