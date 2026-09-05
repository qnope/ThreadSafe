#pragma once

#include <meta>
#include <type_traits>

namespace threadsafe::detail {

template<typename T>
struct is_smart_pointer : std::false_type {};

template<typename T>
struct is_smart_pointer<std::shared_ptr<T>> : std::true_type {};

template<typename T>
struct is_smart_pointer<std::weak_ptr<T>> : std::true_type {};

template<typename T>
struct is_smart_pointer<std::unique_ptr<T>> : std::true_type {};

template <class T>
concept smart_pointer = is_smart_pointer<T>::value;

inline consteval bool trait_value(std::meta::info trait,
                                  std::meta::info type) {
    return std::meta::extract<bool>(std::meta::substitute(trait, {type}));
}

template <class T>
consteval bool compute_dynamic_type_is_known() {
    if constexpr (std::is_void_v<T>)
        return false;
    else if constexpr (!std::meta::is_complete_type(^^T))
        return false;
    else if constexpr (std::is_polymorphic_v<T> && !std::is_final_v<T>)
        return false;
    else
        return true;
}

template <class T>
constexpr bool dynamic_type_is_known = compute_dynamic_type_is_known<T>();

inline consteval bool has_unreflectable_state(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();
    return !std::meta::is_empty_type(type)
        && !std::meta::is_polymorphic_type(type)
        && std::meta::bases_of(type, context).empty()
        && std::meta::nonstatic_data_members_of(type, context).empty();
}

inline consteval bool all_bases_and_members(std::meta::info type,
                                            bool (*question)(std::meta::info)) {
    const auto context = std::meta::access_context::unchecked();

    for (std::meta::info base : std::meta::bases_of(type, context))
        if (!question(std::meta::type_of(base)))
            return false;

    for (std::meta::info member :
         std::meta::nonstatic_data_members_of(type, context))
        if (!question(std::meta::remove_cv(std::meta::type_of(member))))
            return false;

    return true;
}

inline consteval bool is_default_type(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();

    for (std::meta::info member : std::meta::members_of(type, context)) {
        const bool may_hijack_copy_move =
            std::meta::is_constructor_template(member)
            || (std::meta::is_operator_function_template(member)
                && std::meta::operator_of(member) == std::meta::op_equals);
        if (may_hijack_copy_move)
            return false;

        const bool is_copy_move_destroy =
            std::meta::is_copy_constructor(member)
            || std::meta::is_move_constructor(member)
            || std::meta::is_copy_assignment(member)
            || std::meta::is_move_assignment(member)
            || std::meta::is_destructor(member);
        if (is_copy_move_destroy
            && !std::meta::is_defaulted(member)
            && !std::meta::is_deleted(member))
            return false;
    }

    return true;
}

inline consteval bool is_walkable_class(std::meta::info type) {
    return (std::meta::is_class_type(type) || std::meta::is_union_type(type))
        && std::meta::is_complete_type(type)
        && is_default_type(type)
        && !has_unreflectable_state(type);
}

}
