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

// Only the mutable keyword, and only along bases, by-value members and arrays:
// a mutable behind a pointer is out of reach.
inline consteval bool has_mutable_state(std::meta::info type) {
    const auto ctx = std::meta::access_context::unchecked();
    type = std::meta::remove_all_extents(std::meta::remove_cv(type));

    if (!std::meta::is_class_type(type) && !std::meta::is_union_type(type))
        return false;

    if (!std::meta::is_complete_type(type))
        throw std::meta::exception(
            u8"has_mutable_state requires a complete type", type);

    // [res.on.data.races]: a standard library const member function does not
    // modify what other threads can reach, so those internals are race-free by
    // fiat — reflecting them only reports implementation noise, never the
    // element types, which are behind pointers. The template arguments are.
    if (std::meta::parent_of(type) == ^^std) {
        if (std::meta::has_template_arguments(type))
            for (std::meta::info a : std::meta::template_arguments_of(type))
                if (std::meta::is_type(a) && has_mutable_state(a))
                    return true;
        return false;
    }

    for (std::meta::info b : std::meta::bases_of(type, ctx))
        if (has_mutable_state(std::meta::type_of(b)))
            return true;

    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, ctx))
        if (std::meta::is_mutable_member(m)
            || has_mutable_state(std::meta::type_of(m)))
            return true;

    return false;
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
