#pragma once

#include <meta>
#include <type_traits>

namespace threadsafe::detail {

inline consteval bool trait_value(std::meta::info trait, std::meta::info type) {
    return std::meta::extract<bool>(std::meta::substitute(trait, {type}));
}

// Mostly for closure type.
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
