#pragma once

#include <meta>
#include <type_traits>

#include <threadsafe/details/synchronizable_base.h>
#include <threadsafe/details/utils.h>

namespace threadsafe {

template <class T>
struct is_unsafe_sendable : std::false_type {};

template <class T>
constexpr bool is_unsafe_sendable_v = is_unsafe_sendable<T>::value;

inline consteval bool is_unsafe_sendable_type(std::meta::info type) {
    return detail::trait_value(^^is_unsafe_sendable_v, type);
}

namespace detail {
consteval bool diagnose_is_sendable(std::meta::info type);
}

template <class T>
struct is_sendable : std::bool_constant<detail::diagnose_is_sendable(^^T)> {};

template <class T>
constexpr bool is_sendable_v = is_sendable<T>::value;

template <class T>
concept sendable = is_sendable_v<T>;

inline consteval bool is_sendable_type(std::meta::info type) {
    return detail::trait_value(^^is_sendable_v, type);
}

namespace detail {

inline consteval bool diagnose_is_sendable(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();

    if (const auto unqualified = remove_cv(type); unqualified != type)
        return is_sendable_type(unqualified);

    if (is_unsafe_sendable_type(type))
        return true;

    if (is_reference_type(type))
        return is_synchronizable_type(remove_cv(remove_reference(type)));

    if (is_pointer_type(type))
        return is_synchronizable_type(remove_cv(remove_pointer(type)));

    if (is_array_type(type))
        return is_sendable_type(remove_cv(remove_extent(type)));

    if (is_scalar_type(type) || is_synchronizable_type(type))
        return true;

    if (!is_class_type(type) && !is_union_type(type))
        return false;

    if (!is_complete_type(type))
        return false;

    if (!is_default_type(type))
        return false;

    if (has_unreflectable_state(type))
        return false;

    for (info base : bases_of(type, context))
        if (!is_sendable_type(type_of(base)))
            return false;

    for (info member : nonstatic_data_members_of(type, context))
        if (!is_sendable_type(remove_cv(type_of(member))))
            return false;

    return true;
}

}

}
