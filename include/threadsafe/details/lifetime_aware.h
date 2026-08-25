#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <meta>
#include <ranges>
#include <type_traits>

#include <threadsafe/details/allowed_std_wrappers.h>
#include <threadsafe/details/utils.h>

namespace threadsafe {

namespace detail {
consteval void diagnose_default_is_lifetime_aware(std::meta::info type,
                                                  std::u8string path = {});
consteval bool default_is_lifetime_aware(std::meta::info type);
[[noreturn]] consteval void descend_lifetime_aware(std::meta::info inner,
                                                   const std::u8string &path);
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
struct is_lifetime_aware<T[]> : is_lifetime_aware<std::remove_cv_t<T>> {};

template <class T>
struct is_lifetime_aware<std::reference_wrapper<T>> : std::false_type {};

// Ownership is transitive: the control block keeps the T alive, but a T that
// only borrows still borrows.
template <class T>
struct is_lifetime_aware<std::shared_ptr<T>>
    : is_lifetime_aware<std::remove_cv_t<std::remove_all_extents_t<T>>> {};
template <class T>
struct is_lifetime_aware<std::weak_ptr<T>>
    : is_lifetime_aware<std::remove_cv_t<std::remove_all_extents_t<T>>> {};

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

    // Seeding the path with the type is what turns the walk deep: it is the
    // only caller that reads the message.
    detail::descend_lifetime_aware(^^T, detail::type_name(^^T));
}

namespace detail {

// Continue the walk inside `inner`, already reached through `path`. Coming back
// from the walk means `inner` answers false through a specialization the walk
// cannot read — that is itself the reason.
[[noreturn]] inline consteval void
descend_lifetime_aware(std::meta::info inner, const std::u8string &path) {
    diagnose_default_is_lifetime_aware(inner, path);

    reject(inner,
           u8"is not lifetime aware: is_lifetime_aware is specialized to false "
           u8"for it",
           path);
}

// Reject `subject`; while a path is being built, continue into `inner` instead,
// so the message names the root cause and not the first hop. Only
// assert_lifetime_aware seeds a path — see explain_sendable for why the trait
// itself must leave it empty.
[[noreturn]] inline consteval void
explain_lifetime_aware(std::meta::info subject, std::u8string_view reason,
                       std::meta::info inner, const std::u8string &path) {
    if (path.empty())
        reject(subject, reason);

    descend_lifetime_aware(inner, path + path_step(subject));
}

// The trait itself, phrased so that "no" carries its reason. Returning means
// yes; a std::meta::exception carries the reason it is no — default_is_lifetime_aware
// catches it to answer false, while assert_* lets it escape so the reason
// becomes the diagnostic.
inline consteval void
diagnose_default_is_lifetime_aware(std::meta::info type, std::u8string path) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    const auto unqualified = remove_cv(type);

    // A cv-qualified type reaches the primary template even when its
    // unqualified form has a specialization; forward so both agree.
    if (unqualified != type) {
        if (!is_lifetime_aware_type(unqualified))
            explain_lifetime_aware(type, u8"is not lifetime aware", unqualified,
                                   path);
        return;
    }

    // Reached only from assert_lifetime_aware: the trait itself answers these
    // through its own specializations and never lands on the primary template.
    if (is_reference_type(type)
        || (is_pointer_type(type) && !is_function_type(remove_pointer(type))))
        reject(type,
               u8"is a reference or a raw pointer: it borrows its referent "
               u8"instead of keeping it alive — hold the object, or a "
               u8"std::shared_ptr to it",
               path);

    if (is_array_type(type)) {
        const auto element = remove_cv(remove_extent(type));
        if (!is_lifetime_aware_type(element))
            explain_lifetime_aware(
                type, u8"has an element type that is not lifetime aware",
                element, path);
        return;
    }

    if (trait_value(^^std::ranges::borrowed_range, type))
        reject(type,
               u8"is a borrowed range: a view over someone else's storage, it "
               u8"does not keep its elements alive",
               path);

    if (!is_class_type(type) && !is_union_type(type))
        return;

    // A standard wrapper is nothing but its arguments: ask them instead of
    // walking members that only hold pointers to them.
    if (is_allowed_std_wrapper(type)) {
        for (info wrapped : wrapped_types_of(type))
            if (!is_lifetime_aware_type(wrapped))
                explain_lifetime_aware(
                    type, u8"wraps a type that is not lifetime aware", wrapped,
                    path);
        return;
    }

    if (!is_complete_type(type))
        reject(type,
               u8"is incomplete — is_lifetime_aware<T> needs a complete type",
               path);

    if (has_unreflectable_state(type))
        reject(type,
               u8"holds state reflection cannot see (a closure type with "
               u8"captures); specialize is_lifetime_aware to state the "
               u8"intent",
               path);

    for (info base : bases_of(type, context))
        if (!is_lifetime_aware_type(type_of(base)))
            explain_lifetime_aware(base, u8"is not lifetime aware",
                                   type_of(base), path);

    for (info member : nonstatic_data_members_of(type, context)) {
        const auto member_type = remove_cv(type_of(member));
        if (!is_lifetime_aware_type(member_type))
            explain_lifetime_aware(member, u8"is not lifetime aware",
                                   member_type, path);
    }
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
