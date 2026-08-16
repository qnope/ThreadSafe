#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <meta>
#include <ranges>
#include <type_traits>

#include <threadsafe/utils.h>

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
    const auto ctx = std::meta::access_context::unchecked();
    const auto unqualified = std::meta::remove_cv(type);

    // A cv-qualified type reaches the primary template even when its
    // unqualified form has a specialization; forward so both agree.
    if (unqualified != type)
        return is_lifetime_aware_type(unqualified);

    if (trait_value(^^std::ranges::borrowed_range, type))
        return false;

    if (!std::meta::is_class_type(type) && !std::meta::is_union_type(type))
        return true;

    if (!std::meta::is_complete_type(type))
        throw std::meta::exception(
            u8"is_lifetime_aware<T> requires a complete type", type);

    if (has_unreflectable_state(type))
        return false;

    for (std::meta::info b : std::meta::bases_of(type, ctx))
        if (!is_lifetime_aware_type(std::meta::type_of(b)))
            return false;

    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, ctx))
        if (!is_lifetime_aware_type(std::meta::remove_cv(std::meta::type_of(m))))
            return false;

    return true;
}

}

}

#include <threadsafe/containers.h>
#include <threadsafe/smart_pointers.h>
#include <threadsafe/vocabulary.h>
