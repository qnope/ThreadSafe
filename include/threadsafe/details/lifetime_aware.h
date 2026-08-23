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
consteval void diagnose_default_is_lifetime_aware(std::meta::info type);
consteval bool default_is_lifetime_aware(std::meta::info type);
}

template <class T>
struct is_lifetime_aware
    : std::bool_constant<detail::default_is_lifetime_aware(^^T)> {};

template <class T>
constexpr bool is_lifetime_aware_v = is_lifetime_aware<T>::value;

template <class T>
struct is_lifetime_aware<T&> : std::false_type {};
template <class T>
struct is_lifetime_aware<T&&> : std::false_type {};
template <class T>
struct is_lifetime_aware<T*> : std::false_type {};

template <class F>
    requires std::is_function_v<F>
struct is_lifetime_aware<F*> : std::true_type {};

template <class T, std::size_t N>
struct is_lifetime_aware<T[N]> : is_lifetime_aware<std::remove_cv_t<T>> {};

template <class T>
struct is_lifetime_aware<std::reference_wrapper<T>> : std::false_type {};

template <class T>
struct is_lifetime_aware<std::shared_ptr<T>> : std::true_type {};
template <class T>
struct is_lifetime_aware<std::weak_ptr<T>> : std::true_type {};

template <class T>
concept lifetime_aware = is_lifetime_aware_v<T>;

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_lifetime_aware_v<T>, for code written on the reflection side.
inline consteval bool is_lifetime_aware_type(std::meta::info type) {
    return detail::trait_value(^^is_lifetime_aware_v, type);
}

// Same question as `static_assert(is_lifetime_aware_v<T>)`, but a failure names
// the subobject responsible instead of printing a bare false.
template <class T>
consteval void assert_lifetime_aware() {
    if (is_lifetime_aware_v<T>)
        return;

    detail::diagnose_default_is_lifetime_aware(^^T);

    throw std::meta::exception(
        u8"is_lifetime_aware is specialized to false for this type", ^^T);
}

namespace detail {

// The trait itself, phrased so that "no" carries its reason. Returning means
// yes; a std::meta::exception carries the reason it is no — default_is_lifetime_aware
// catches it to answer false, while assert_* lets it escape so the reason
// becomes the diagnostic.
inline consteval void diagnose_default_is_lifetime_aware(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    const auto unqualified = remove_cv(type);

    // A cv-qualified type reaches the primary template even when its
    // unqualified form has a specialization; forward so both agree.
    if (unqualified != type) {
        if (!is_lifetime_aware_type(unqualified))
            reject(type, u8"is not lifetime aware");
        return;
    }

    // Reached only from assert_lifetime_aware: the trait itself answers these
    // through its own specializations and never lands on the primary template.
    if (is_reference_type(type)
        || (is_pointer_type(type) && !is_function_type(remove_pointer(type))))
        throw exception(
            u8"a reference or a raw pointer borrows its referent instead of "
            u8"keeping it alive — hold the object, or a std::shared_ptr to it",
            type);

    if (is_array_type(type)) {
        if (!is_lifetime_aware_type(remove_cv(remove_extent(type))))
            reject(type, u8"has an element type that is not lifetime aware");
        return;
    }

    if (trait_value(^^std::ranges::borrowed_range, type))
        reject(type,
               u8"is a borrowed range: a view over someone else's storage, it "
               u8"does not keep its elements alive");

    if (!is_class_type(type) && !is_union_type(type))
        return;

    if (!is_complete_type(type))
        throw exception(
            u8"is_lifetime_aware<T> requires a complete type", type);

    if (has_unreflectable_state(type))
        reject(type,
               u8"holds state reflection cannot see (a closure type with "
               u8"captures); specialize is_lifetime_aware to state the "
               u8"intent");

    for (info base : bases_of(type, context))
        if (!is_lifetime_aware_type(type_of(base)))
            reject(base, u8"is not lifetime aware");

    for (info member : nonstatic_data_members_of(type, context))
        if (!is_lifetime_aware_type(remove_cv(type_of(member))))
            reject(member, u8"is not lifetime aware");
}

inline consteval bool default_is_lifetime_aware(std::meta::info type) {
    try {
        diagnose_default_is_lifetime_aware(type);
        return true;
    } catch (const std::meta::exception &) {
        return false;
    }
}

}

}
