#pragma once

#include <meta>
#include <string>
#include <string_view>
#include <type_traits>

namespace threadsafe::detail {

inline consteval std::u8string type_name(std::meta::info type) {
    return std::u8string(std::meta::u8display_string_of(type));
}

inline consteval std::u8string member_name(std::meta::info member) {
    if (std::meta::has_identifier(member))
        return std::u8string(std::meta::u8identifier_of(member));
    return u8"<unnamed>";
}

// The subject of a rejection — a data member, a base, or the type itself —
// spelled the way the message opens on it.
inline consteval std::u8string describe(std::meta::info subject) {
    if (std::meta::is_nonstatic_data_member(subject))
        return u8"member `" + member_name(subject) + u8"` of type "
             + type_name(std::meta::type_of(subject));

    if (std::meta::is_base(subject))
        return u8"base class " + type_name(std::meta::type_of(subject));

    return type_name(subject);
}

// One hop of the walk down to the culprit, named the way the user wrote it: the
// member or the base, with the type it stands for.
inline consteval std::u8string path_step(std::meta::info subject) {
    if (std::meta::is_nonstatic_data_member(subject))
        return u8"::" + member_name(subject) + u8" ("
             + type_name(std::meta::type_of(subject)) + u8")";

    if (std::meta::is_base(subject))
        return u8"::(base " + type_name(std::meta::type_of(subject)) + u8")";

    // The subject is the type itself — a cv-qualified spelling, or an array of
    // the type walked next. The same object under another name: no step.
    return {};
}

// The reason continues the sentence the subject opens: reject(member, u8"is not
// sendable") reads as "member `borrowed` of type int * is not sendable".
//
// A path opens that sentence instead — its last step already names the subject,
// and it names every step taken to reach it: "Error::ptr (IntPtr)::ptr (int *)
// is not sendable".
[[noreturn]] inline consteval void reject(std::meta::info subject,
                                          std::u8string_view reason,
                                          std::u8string_view path = {}) {
    throw std::meta::exception(
        (path.empty() ? describe(subject) : std::u8string(path)) + u8" "
            + std::u8string(reason),
        subject);
}

// Reject a subobject the path has not stepped onto yet, on a reason that is
// terminal — nothing deeper to walk, so the path stops here.
[[noreturn]] inline consteval void reject_at(std::meta::info subject,
                                             std::u8string_view reason,
                                             const std::u8string &path) {
    if (path.empty())
        reject(subject, reason);

    reject(subject, reason, path + path_step(subject));
}

// A structural trait walks the members of the *static* type. Through an
// indirection the object may be of a derived type, whose extra members the walk
// never saw, so a structural answer about a polymorphic non-final pointee proves
// nothing about the object actually there.
//
// std::is_polymorphic and std::is_final are ill-formed on an incomplete type, so
// they are asked only once completeness is known. An incomplete pointee cannot be
// judged at all, which is exactly the case this guard exists for.
template <class T>
consteval bool compute_dynamic_type_is_known() {
    // void erases the type outright: the object behind it is of some other
    // type entirely, and nothing here names it. That is the question this
    // guard asks, so the answer is no.
    if constexpr (std::is_void_v<T>)
        return false;
    else if constexpr (!std::meta::is_complete_type(^^T))
        return false;
    else
        return !std::is_polymorphic_v<T> || std::is_final_v<T>;
}

template <class T>
constexpr bool dynamic_type_is_known = compute_dynamic_type_is_known<T>();

inline consteval bool trait_value(std::meta::info trait, std::meta::info type) {
    return std::meta::extract<bool>(std::meta::substitute(trait, {type}));
}

// Mostly for closure type.
inline consteval bool has_unreflectable_state(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();
    return !std::meta::is_empty_type(type)
        && !std::meta::is_polymorphic_type(type)
        && std::meta::bases_of(type, context).empty()
        && std::meta::nonstatic_data_members_of(type, context).empty();
}

inline consteval bool is_copy_move_destroy_member(std::meta::info member) {
    return std::meta::is_copy_constructor(member)
        || std::meta::is_move_constructor(member)
        || std::meta::is_copy_assignment(member)
        || std::meta::is_move_assignment(member)
        || std::meta::is_destructor(member);
}

// A template is never a copy or move member, but it can still be *selected* for
// a copy or a move. Against a non-const lvalue `T&`, a `template <class U>
// T(U&&)` deduces `U = T&` and matches exactly, where the implicit
// `T(const T&)` needs a qualification conversion; the non-template tiebreaker
// never runs, and `T b = a;` calls user code although every special member is
// implicit. Same for `template <class U> T& operator=(U&&)`. (`const U&` and
// by-value forms tie with the special member and lose the tiebreaker, so only
// the deduce-to-`T&` shapes hijack.)
//
// Which shape it is cannot be told from here: parameters_of rejects a template,
// so an arity or a constraint that makes hijacking impossible — `T(It, It)`, or
// a `requires !same_as<remove_cvref_t<U>, T>` — is indistinguishable from a
// greedy forwarding constructor. Any such template therefore blocks the
// default; write the special members out, or specialize the trait.
inline consteval bool may_hijack_copy_move(std::meta::info member) {
    return std::meta::is_constructor_template(member)
        || (std::meta::is_operator_function_template(member)
            && std::meta::operator_of(member) == std::meta::op_equals);
}

inline consteval bool
has_only_default_copy_move_destroy(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();
    for (std::meta::info member : std::meta::members_of(type, context)) {
        if (may_hijack_copy_move(member))
            return false;

        if (!is_copy_move_destroy_member(member))
            continue;

        if (!std::meta::is_defaulted(member) && !std::meta::is_deleted(member))
            return false;
    }
    return true;
}

}
