#pragma once

#include <atomic>
#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/details/allowed_std_wrappers.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable_base.h>

namespace threadsafe {

template <class F>
concept function_type = std::is_function_v<F>;

template <function_type F>
struct is_synchronizable<F> : std::true_type {};

template <class T>
struct is_synchronizable<std::atomic<T>> : is_sendable<T> {};

namespace detail {
consteval void
diagnose_default_is_const_synchronizable(std::meta::info type,
                                         std::u8string path = {});
consteval bool default_is_const_synchronizable(std::meta::info type);
[[noreturn]] consteval void
descend_const_synchronizable(std::meta::info inner, const std::u8string &path);
}

// Thread-safe read: a const T may be read from several threads at once. Full
// synchronizability implies it; otherwise the same structural guard as
// is_sendable applies, and every subobject reachable through the const must be
// read-safe in turn — a mutable member is writable through const and needs the
// full trait, and const behind an indirection is never trusted: the pointee
// may have been reached through a non-const alias at origin.
template <class T>
struct is_synchronizable<const T>
    : std::bool_constant<detail::default_is_const_synchronizable(^^T)> {};

// The const array forms exist because <const T> above matches a const array
// and would otherwise tie with the <T[N]> rule of synchronizable_base.h.
template <class T, std::size_t N>
struct is_synchronizable<const T[N]> : is_synchronizable<const T> {};
template <class T>
struct is_synchronizable<const T[]> : is_synchronizable<const T> {};

// Same question as `static_assert(is_synchronizable_v<T>)`, but a failure names
// the subobject responsible instead of printing a bare false.
template <class T>
consteval void assert_synchronizable() {
    if (is_synchronizable_v<T>)
        return;

    // Only the const question has a structural answer to walk; the full trait
    // is opt-in, so a non-const T has nothing to explain beyond that.
    if (!std::is_const_v<T>)
        throw std::meta::exception(
            u8"is_synchronizable<T> is opt-in: it holds only for types that "
            u8"synchronize themselves (std::atomic, a mutex-protected "
            u8"wrapper). Ask is_synchronizable<const T> for a read-only share, "
            u8"or use THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE to vouch for it",
            ^^T);

    // Seeding the path with the type is what turns the walk deep: it is the
    // only caller that reads the message.
    detail::descend_const_synchronizable(^^T, detail::type_name(^^T));
}

namespace detail {

// Continue the walk inside `inner`, already reached through `path`. Coming back
// from the walk means `inner` answers false through a specialization the walk
// cannot read — that is itself the reason.
[[noreturn]] inline consteval void
descend_const_synchronizable(std::meta::info inner, const std::u8string &path) {
    diagnose_default_is_const_synchronizable(inner, path);

    reject(inner,
           u8"is not readable from several threads at once: is_synchronizable "
           u8"is specialized to false for it",
           path);
}

// Reject `subject`; while a path is being built, continue into `inner` instead,
// so the message names the root cause and not the first hop. Only
// assert_synchronizable seeds a path — see explain_sendable for why the trait
// itself must leave it empty.
[[noreturn]] inline consteval void
explain_const_synchronizable(std::meta::info subject, std::u8string_view reason,
                             std::meta::info inner, const std::u8string &path) {
    if (path.empty())
        reject(subject, reason);

    descend_const_synchronizable(inner, path + path_step(subject));
}

// The trait itself, phrased so that "no" carries its reason. Returning means
// yes; a std::meta::exception carries the reason it is no — default_is_const_synchronizable
// catches it to answer false, while assert_* lets it escape so the reason
// becomes the diagnostic.
inline consteval void
diagnose_default_is_const_synchronizable(std::meta::info type,
                                         std::u8string path) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    type = remove_cv(type);

    if (is_synchronizable_type(type))
        return;

    // A pointee's const is a view restriction, not an object property — the
    // object may be written through another alias, so the full trait is asked.
    // A function pointee is code, and code is synchronizable.
    if (is_pointer_type(type)) {
        const auto pointee = remove_cv(remove_pointer(type));
        if (!is_synchronizable_type(pointee))
            reject(type,
                   u8"is a pointer: the const stops at it — the pointee may be "
                   u8"written through another alias, so the pointee must be "
                   u8"synchronizable itself",
                   path);
        return;
    }

    if (is_array_type(type)) {
        const auto element = remove_cv(remove_extent(type));
        if (!is_synchronizable_type(add_const(element)))
            explain_const_synchronizable(
                type,
                u8"has an element type that is not readable from several "
                u8"threads at once",
                element, path);
        return;
    }

    if (is_scalar_type(type))
        return;

    if (is_void_type(type))
        reject(type, u8"holds no value to read", path);

    if (!is_class_type(type) && !is_union_type(type))
        reject(type,
               u8"is not a scalar, class or union type — "
               u8"is_synchronizable<const T> supports no others",
               path);

    // [res.on.data.races]: the const member functions of a standard container
    // may run concurrently, so a const wrapper is read-safe exactly when
    // everything a reader reaches through it — elements and stored policies —
    // is. Reading the arguments also keeps the recursion out of libstdc++
    // internals, whose mutable members (unordered_*'s rehash policy) are
    // covered by that guarantee.
    if (is_allowed_std_wrapper(type)) {
        for (info wrapped : wrapped_types_of(type))
            if (!is_synchronizable_type(add_const(wrapped)))
                explain_const_synchronizable(
                    type,
                    u8"wraps a type that is not readable from several threads "
                    u8"at once",
                    wrapped, path);
        return;
    }

    if (!is_complete_type(type))
        reject(type,
               u8"is incomplete — is_synchronizable<const T> needs a complete "
               u8"type; specialize is_synchronizable for types holding a "
               u8"pointer to an incomplete type (the pimpl idiom)",
               path);

    if (!has_only_default_copy_move_destroy(type))
        reject(type,
               u8"has a user-written copy, move or destructor — or a template "
               u8"that may be selected as one — which can share state the "
               u8"members do not show; specialize is_synchronizable to state "
               u8"the intent",
               path);

    if (has_unreflectable_state(type))
        reject(type,
               u8"holds state reflection cannot see (a closure type with "
               u8"captures); specialize is_synchronizable to state the "
               u8"intent",
               path);

    for (info base : bases_of(type, context))
        if (!is_synchronizable_type(add_const(type_of(base))))
            explain_const_synchronizable(
                base, u8"is not readable from several threads at once",
                type_of(base), path);

    for (info member : nonstatic_data_members_of(type, context)) {
        const auto member_type = type_of(member);
        if (is_mutable_member(member)) {
            // mutable defeats const: this member is writable through a const&, so it
            // needs the full (write-safe) trait, not the const one.
            if (!is_synchronizable_type(remove_cv(member_type)))
                reject_at(member,
                          u8"is mutable, so it is written through a const "
                          u8"reference: its type must be fully synchronizable",
                          path);
        } else if (is_reference_type(member_type)) {
            // a reference member's constness is unrelated to the referent's; the
            // referent may be shared and mutated through another alias.
            if (!is_synchronizable_type(remove_cvref(member_type)))
                reject_at(member,
                          u8"is a reference: the const stops there — its "
                          u8"referent may be written through another alias, so "
                          u8"the referent must be synchronizable itself",
                          path);
        } else if (!is_synchronizable_type(add_const(member_type))) {
            // ordinary value member: const propagates normally.
            explain_const_synchronizable(
                member, u8"is not readable from several threads at once",
                remove_cv(member_type), path);
        }
    }
}

inline consteval bool default_is_const_synchronizable(std::meta::info type) {
    try {
        diagnose_default_is_const_synchronizable(type);
        return true;
    } catch (const std::meta::exception &) {
        return false;
    }
}

}

}
