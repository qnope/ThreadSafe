#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/details/synchronizable_base.h>
#include <threadsafe/details/utils.h>

namespace threadsafe {

namespace detail {
consteval void diagnose_default_is_sendable(std::meta::info type,
                                            std::u8string path = {});
consteval bool default_is_sendable(std::meta::info type);
[[noreturn]] consteval void descend_sendable(std::meta::info inner,
                                             const std::u8string &path);
}

template <class T>
struct is_sendable : std::bool_constant<detail::default_is_sendable(^^T)> {};

template <class T>
constexpr bool is_sendable_v = is_sendable<T>::value;

template <class T>
struct is_sendable<T&> : is_synchronizable<std::remove_cv_t<T>> {};
template <class T>
struct is_sendable<T&&> : is_synchronizable<std::remove_cv_t<T>> {};

template <class T>
struct is_sendable<T*> : is_synchronizable<std::remove_cv_t<T>> {};

template <class T, std::size_t N>
struct is_sendable<T[N]> : is_sendable<std::remove_cv_t<T>> {};
template <class T>
struct is_sendable<T[]> : is_sendable<std::remove_cv_t<T>> {};

template <class T>
concept sendable = is_sendable_v<T>;

// The info-level face of the trait, named after the predicates of <meta>. Same
// answer as is_sendable_v<T>, for code written on the reflection side.
inline consteval bool is_sendable_type(std::meta::info type) {
    return detail::trait_value(^^is_sendable_v, type);
}

// Same question as `static_assert(is_sendable_v<T>)`, but a failure names the
// subobject responsible instead of printing a bare false.
template <class T>
consteval void assert_sendable() {
    if (is_sendable_v<T>)
        return;

    // Seeding the path with the type is what turns the walk deep: it is the
    // only caller that reads the message.
    detail::descend_sendable(^^T, detail::type_name(^^T));
}

namespace detail {

template <class T>
constexpr bool dynamic_type_is_known =
    !std::is_polymorphic_v<T> || std::is_final_v<T>;

// Continue the walk inside `inner`, already reached through `path`. Coming back
// from the walk means `inner` answers false through a specialization the walk
// cannot read — that is itself the reason.
[[noreturn]] inline consteval void descend_sendable(std::meta::info inner,
                                                    const std::u8string &path) {
    diagnose_default_is_sendable(inner, path);

    reject(inner,
           u8"is not sendable: is_sendable is specialized to false for it",
           path);
}

// Reject `subject`; while a path is being built, continue into `inner` instead,
// so the message names the root cause and not the first hop.
//
// Only assert_sendable seeds a path, because it is the only caller that reads
// the message. The trait leaves it empty and pays nothing: walking every
// subobject a second time to word a message nobody reads would make each
// "false" answer quadratic — measured at 38x on a 60-level chain.
[[noreturn]] inline consteval void explain_sendable(std::meta::info subject,
                                                    std::u8string_view reason,
                                                    std::meta::info inner,
                                                    const std::u8string &path) {
    if (path.empty())
        reject(subject, reason);

    descend_sendable(inner, path + path_step(subject));
}

// The trait itself, phrased so that "no" carries its reason. Returning means
// yes; a std::meta::exception carries the reason it is no — default_is_sendable
// catches it to answer false, while assert_* lets it escape so the reason
// becomes the diagnostic.
inline consteval void diagnose_default_is_sendable(std::meta::info type,
                                                   std::u8string path) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    const auto unqualified = remove_cv(type);

    // A cv-qualified type reaches the primary template even when its
    // unqualified form has a specialization; forward so both agree.
    if (unqualified != type) {
        if (!is_sendable_type(unqualified))
            explain_sendable(type, u8"is not sendable", unqualified, path);
        return;
    }

    // Reached only from assert_sendable: the trait itself answers these through
    // its own specializations and never lands on the primary template.
    if (is_pointer_type(type) || is_reference_type(type))
        reject(type,
               u8"is a pointer or a reference: sending it shares its referent "
               u8"with the other thread, so the referent must be "
               u8"synchronizable — and synchronizability is opt-in",
               path);

    if (is_array_type(type)) {
        const auto element = remove_cv(remove_extent(type));
        if (!is_sendable_type(element))
            explain_sendable(type, u8"has an element type that is not sendable",
                             element, path);
        return;
    }

    if (is_synchronizable_type(type) || is_scalar_type(type))
        return;

    if (is_void_type(type))
        reject(type, u8"holds no value to send", path);

    if (!is_class_type(type) && !is_union_type(type))
        reject(type,
               u8"is not a scalar, class or union type — is_sendable<T> "
               u8"supports no others",
               path);

    if (!is_complete_type(type))
        reject(type,
               u8"is incomplete — is_sendable<T> needs a complete type; "
               u8"specialize is_sendable for types holding a pointer to an "
               u8"incomplete type (the pimpl idiom)",
               path);

    if (!has_only_default_copy_move_destroy(type))
        reject(type,
               u8"has a user-written copy, move or destructor — or a template "
               u8"that may be selected as one — which can share state the "
               u8"members do not show; specialize is_sendable to state the "
               u8"intent",
               path);

    if (has_unreflectable_state(type))
        reject(type,
               u8"holds state reflection cannot see (a closure type with "
               u8"captures); specialize is_sendable to state the intent",
               path);

    for (info base : bases_of(type, context))
        if (!is_sendable_type(type_of(base)))
            explain_sendable(base, u8"is not sendable", type_of(base), path);

    for (info member : nonstatic_data_members_of(type, context)) {
        const auto member_type = remove_cv(type_of(member));
        if (!is_sendable_type(member_type))
            explain_sendable(member, u8"is not sendable", member_type, path);
    }
}

inline consteval bool default_is_sendable(std::meta::info type) {
    try {
        diagnose_default_is_sendable(type);
        return true;
    } catch (const std::meta::exception &) {
        return false;
    }
}

}

}
