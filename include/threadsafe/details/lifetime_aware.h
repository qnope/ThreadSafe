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
consteval TraitAnswer diagnose_is_lifetime_aware(std::meta::info type);
}

template <class T>
struct is_lifetime_aware
{
    static constexpr TraitAnswer value = detail::diagnose_is_lifetime_aware(^^T);
};

template <class T>
constexpr TraitAnswer is_lifetime_aware_v = is_lifetime_aware<T>::value;

template <class T>
struct is_lifetime_aware<T&> {
    static constexpr TraitAnswer value = "References borrow their referent instead of keeping it alive";
};

template <class T>
struct is_lifetime_aware<T&&> {
    static constexpr TraitAnswer value = "References borrow their referent instead of keeping it alive";
};

template <class T>
struct is_lifetime_aware<T*> {
    static constexpr TraitAnswer value = "Raw pointers borrow their pointee instead of keeping it alive";
};

template <class F>
    requires std::is_function_v<F>
struct is_lifetime_aware<F*> {
    static constexpr TraitAnswer value = {};
};

template <class T>
struct is_lifetime_aware<std::reference_wrapper<T>> {
    static constexpr TraitAnswer value = "std::reference_wrapper borrows its referent instead of keeping it alive";
};

template <class T, std::size_t N>
struct is_lifetime_aware<T[N]> : is_lifetime_aware<std::remove_cv_t<T>> {};

template <class T>
struct is_lifetime_aware<T[]> : is_lifetime_aware<std::remove_cv_t<T>> {};

// Ownership is transitive: the control block keeps the T alive, but a T that
// only borrows still borrows -- and that answer is read off the static type, so
// the dynamic type must be known for it to hold of the object actually pointed to.
template <class T>
struct is_lifetime_aware<std::shared_ptr<T>> {
    using pointee = std::remove_cv_t<std::remove_all_extents_t<T>>;

    static constexpr TraitAnswer value = [] {
        if (const auto answer = is_lifetime_aware_v<pointee>; !answer)
            return answer;

        return detail::dynamic_type_is_known<pointee>;
    }();
};

template <class T>
struct is_lifetime_aware<std::weak_ptr<T>> {
    using pointee = std::remove_cv_t<std::remove_all_extents_t<T>>;

    static constexpr TraitAnswer value = [] {
        if (const auto answer = is_lifetime_aware_v<pointee>; !answer)
            return answer;

        return detail::dynamic_type_is_known<pointee>;
    }();
};

template <class T>
concept lifetime_aware = bool(is_lifetime_aware_v<T>);

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_lifetime_aware_v<T>, for code written on the reflection side.
inline consteval TraitAnswer is_lifetime_aware_type(std::meta::info type) {
    return detail::trait_value(^^is_lifetime_aware_v, type);
}

namespace detail {

// The structural default: a type keeps its data alive when every base and every
// member does. A default-constructed answer means yes; otherwise it says why not.
inline consteval TraitAnswer diagnose_is_lifetime_aware(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    const auto unqualified = remove_cv(type);

    // A cv-qualified type reaches the primary template even when its
    // unqualified form has a specialization; forward so both agree.
    if (unqualified != type)
        return is_lifetime_aware_type(unqualified);

    if (is_array_type(type))
        return is_lifetime_aware_type(remove_cv(remove_extent(type)));

    if (is_void_type(type))
        return "void holds no value to own";

    if (extract<bool>(substitute(^^std::ranges::borrowed_range, {type})))
        return "is a borrowed range: a view over someone else's storage, it "
                  "does not keep its elements alive";

    // A scalar owns whatever it is; the borrowing shapes -- references, raw
    // pointers -- answer through their own specializations above.
    if (!is_class_type(type) && !is_union_type(type))
        return {};

    if (!is_complete_type(type))
        return "is incomplete — is_lifetime_aware<T> needs a complete "
                  "type";

    if (has_unreflectable_state(type))
        return "holds state reflection cannot see (a closure type with "
                  "captures); specialize is_lifetime_aware to state the "
                  "intent";

    for (info base : bases_of(type, context))
        if (!is_lifetime_aware_type(type_of(base)))
            return "a base class borrows instead of keeping its data "
                      "alive";

    for (info member : nonstatic_data_members_of(type, context))
        if (!is_lifetime_aware_type(remove_cv(type_of(member))))
            return "a member borrows instead of keeping its data alive";

    return {};
}

}

}
