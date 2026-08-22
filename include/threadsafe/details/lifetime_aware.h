#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <meta>
#include <ranges>
#include <type_traits>

#include <threadsafe/details/utils.h>

namespace threadsafe {

namespace detail {
consteval bool default_is_lifetime_aware(std::meta::info type);
}

template <class T>
constexpr bool is_lifetime_aware = detail::default_is_lifetime_aware(^^T);

template <class T>
constexpr bool is_lifetime_aware<T&> = false;
template <class T>
constexpr bool is_lifetime_aware<T&&> = false;
template <class T>
constexpr bool is_lifetime_aware<T*> = false;

template <class F>
    requires std::is_function_v<F>
constexpr bool is_lifetime_aware<F*> = true;

template <class T, std::size_t N>
constexpr bool is_lifetime_aware<T[N]> = is_lifetime_aware<std::remove_cv_t<T>>;

template <class T>
constexpr bool is_lifetime_aware<std::reference_wrapper<T>> = false;

template <class T>
constexpr bool is_lifetime_aware<std::shared_ptr<T>> = true;
template <class T>
constexpr bool is_lifetime_aware<std::weak_ptr<T>> = true;

template <class T>
concept lifetime_aware = is_lifetime_aware<T>;

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_lifetime_aware<T>, for code written on the reflection side.
inline consteval bool is_lifetime_aware_type(std::meta::info type) {
    return detail::trait_value(^^is_lifetime_aware, type);
}

namespace detail {

inline consteval bool default_is_lifetime_aware(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    const auto unqualified = remove_cv(type);

    // A cv-qualified type reaches the primary template even when its
    // unqualified form has a specialization; forward so both agree.
    if (unqualified != type)
        return is_lifetime_aware_type(unqualified);

    if (trait_value(^^std::ranges::borrowed_range, type))
        return false;

    if (!is_class_type(type) && !is_union_type(type))
        return true;

    if (!is_complete_type(type))
        throw exception(
            u8"is_lifetime_aware<T> requires a complete type", type);

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
