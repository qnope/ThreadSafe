#pragma once

#include <meta>
#include <string_view>
#include <type_traits>

namespace threadsafe {

// What every trait answers: yes, or the reason it is no.
//
// The reason is a plain pointer into static storage, and the promotion is what
// puts it there: std::meta::extract reads a trait's answer back out of the
// substituted variable template, and it only reads a structural type — which a
// std::string_view, holding its two members privately, is not.
struct TraitAnswer {
    constexpr TraitAnswer() = default;

    consteval TraitAnswer(const char *reason)
        : error_message(std::define_static_string(std::string_view(reason))) {}

    constexpr explicit operator bool() const {
        return error_message == nullptr;
    }

    const char *error_message = nullptr;
};

}

namespace threadsafe::detail {

inline consteval TraitAnswer trait_value(std::meta::info trait,
                                         std::meta::info type) {
    return std::meta::extract<TraitAnswer>(std::meta::substitute(trait, {type}));
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
consteval TraitAnswer compute_dynamic_type_is_known() {
    // void erases the type outright: the object behind it is of some other
    // type entirely, and nothing here names it. That is the question this
    // guard asks, so the answer is no.
    if constexpr (std::is_void_v<T>)
        return "points at a void: nothing here names the object actually "
               "there";
    else if constexpr (!std::meta::is_complete_type(^^T))
        return "points at an incomplete type, so the object actually there "
               "cannot be judged at all";
    else if constexpr (std::is_polymorphic_v<T> && !std::is_final_v<T>)
        return "points at a polymorphic non-final type: the object actually "
               "there may be of a derived type, whose members the walk never "
               "saw; make the pointee final, or specialize the trait";
    else
        return {};
}

template <class T>
constexpr TraitAnswer dynamic_type_is_known = compute_dynamic_type_is_known<T>();

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

inline consteval TraitAnswer is_default_type(std::meta::info type) {
    const auto context = std::meta::access_context::unchecked();

    for (std::meta::info member : std::meta::members_of(type, context)) {
        if (may_hijack_copy_move(member))
            return "a constructor or assignment template may be selected as "
                      "a copy or a move; write the special members out, or "
                      "specialize the trait";

        if (!is_copy_move_destroy_member(member))
            continue;

        if (!std::meta::is_defaulted(member) && !std::meta::is_deleted(member))
            return "a user-written copy, move or destructor can share state "
                      "the members do not show; specialize the trait to state "
                      "the intent";
    }

    return {};
}

}
