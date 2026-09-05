#pragma once

#include <functional>
#include <memory>
#include <meta>
#include <ranges>
#include <type_traits>

#include <threadsafe/details/utils.h>

namespace threadsafe {

template <class T>
struct is_unsafe_lifetime_aware : std::false_type {};

template<class T>
constexpr bool is_unsafe_lifetime_aware_v = is_unsafe_lifetime_aware<T>::value;

inline consteval bool
is_unsafe_lifetime_aware_type(std::meta::info type) {
    return detail::trait_value(^^is_unsafe_lifetime_aware_v, type);
}

namespace detail {
consteval bool diagnose_is_lifetime_aware(std::meta::info type);
}

template <class T>
struct is_lifetime_aware
{
    static constexpr bool value = detail::diagnose_is_lifetime_aware(^^T);
};

template <class T>
constexpr bool is_lifetime_aware_v = is_lifetime_aware<T>::value;

template <class T>
concept lifetime_aware = is_lifetime_aware_v<T>;

inline consteval bool is_lifetime_aware_type(std::meta::info type) {
    return detail::trait_value(^^is_lifetime_aware_v, type);
}

namespace detail {

template <class T>
consteval bool pointee_is_lifetime_aware() {
    using pointee = typename T::element_type;
    return is_lifetime_aware_v<pointee> && dynamic_type_is_known<pointee>;
}

}

template <detail::smart_pointer T>
struct is_lifetime_aware<T>
    : std::bool_constant<detail::pointee_is_lifetime_aware<T>()> {};

template <class T> requires (std::is_reference_v<T> || std::is_pointer_v<T>)
struct is_lifetime_aware<T> : std::false_type {};

template<class T> requires (std::is_function_v<T>)
struct is_lifetime_aware<T *> : std::true_type
{};

template <class T> requires (std::is_array_v<T>)
struct is_lifetime_aware<T> : is_lifetime_aware<std::remove_all_extents_t<T>>{};

namespace detail {

inline consteval bool diagnose_is_lifetime_aware(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    const auto unqualified = remove_cv(type);

    if (unqualified != type)
        return is_lifetime_aware_type(unqualified);

    if (is_unsafe_lifetime_aware_type(type))
        return true;

    if (is_void_type(type))
        return false;

    if (extract<bool>(substitute(^^std::ranges::borrowed_range, {type})))
        return false;

    if (!is_class_type(type) && !is_union_type(type))
        return true;

    if (!is_complete_type(type))
        return false;

    if (has_unreflectable_state(type))
        return false;

    for (info base : bases_of(type, context))
        if (!is_lifetime_aware_type(type_of(base)))
            return false;

    for (info member : nonstatic_data_members_of(type, context))
        if (!is_lifetime_aware_type(remove_cv(type_of(member))))
            return false;

    return true;
}

}

}
