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

inline consteval bool is_copy_move_destroy_member(std::meta::info m) {
    return std::meta::is_copy_constructor(m)
        || std::meta::is_move_constructor(m)
        || std::meta::is_copy_assignment(m)
        || std::meta::is_move_assignment(m)
        || std::meta::is_destructor(m);
}

// A template is never a copy or move member, but it can still be *selected* for
// a copy or a move. Against a non-const lvalue `T&`, a `template <class U>
// T(U&&)` deduces `U = T&` and matches exactly, where the implicit
// `T(const T&)` needs a qualification conversion; the non-template tiebreaker
// never runs, and `T b = a;` calls user code although every special member is
// implicit. Same for `template <class U> T& operator=(U&&)`. (`const U&` and
// by-value forms tie with the special member and lose the tiebreaker, so only
// the deduce-to-`T&` shapes hijack.)
//
// Which shape it is cannot be told from here: parameters_of rejects a template,
// so an arity or a constraint that makes hijacking impossible — `T(It, It)`, or
// a `requires !same_as<remove_cvref_t<U>, T>` — is indistinguishable from a
// greedy forwarding constructor. Any such template therefore blocks the
// default; write the special members out, or specialize the trait.
inline consteval bool may_hijack_copy_move(std::meta::info m) {
    return std::meta::is_constructor_template(m)
        || (std::meta::is_operator_function_template(m)
            && std::meta::operator_of(m) == std::meta::op_equals);
}

inline consteval bool
has_only_default_copy_move_destroy(std::meta::info type) {
    const auto ctx = std::meta::access_context::unchecked();
    for (std::meta::info m : std::meta::members_of(type, ctx)) {
        if (may_hijack_copy_move(m))
            return false;

        if (!is_copy_move_destroy_member(m))
            continue;

        if (!std::meta::is_defaulted(m) && !std::meta::is_deleted(m))
            return false;
    }
    return true;
}

}
