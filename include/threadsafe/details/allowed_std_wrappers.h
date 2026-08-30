#pragma once

#include <algorithm>
#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <meta>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>
#include <threadsafe/details/synchronizable.h>

namespace threadsafe {

namespace detail {

// The standard templates that add no state of their own: a specialization is
// nothing but its arguments, held by value. The three rules at the bottom of
// this file therefore read the arguments instead of letting the structural walk
// see the members — a std::vector<T> holds a T*, not a T, and its constructor
// templates block the structural default anyway (see may_hijack_copy_move).
//
// An allow-list, not a deduction: nothing in reflection tells std::vector<T>
// apart from a type that hides sharing behind the same arguments. This list is
// only the membership test; the answer itself is written the way every other
// answer in this library is written — as a specialization of the trait.
inline constexpr std::meta::info allowed_std_wrappers[] = {
    ^^std::vector,        ^^std::deque,             ^^std::list,
    ^^std::forward_list,  ^^std::basic_string,      ^^std::map,
    ^^std::multimap,      ^^std::set,               ^^std::multiset,
    ^^std::unordered_map, ^^std::unordered_multimap,
    ^^std::unordered_set, ^^std::unordered_multiset,
    ^^std::pair,          ^^std::tuple,             ^^std::optional,
    ^^std::variant,       ^^std::array,
};

inline consteval bool is_allowed_std_wrapper(std::meta::info type) {
    type = std::meta::dealias(type);
    return std::meta::has_template_arguments(type)
        && std::ranges::contains(allowed_std_wrappers,
                                 std::meta::template_of(type));
}

// The family as a concept: what the three specializations are keyed on. A
// constrained partial specialization over the same argument list as the primary
// template — the shape is_synchronizable<F> already takes for function types.
template <class T>
concept std_wrapper = is_allowed_std_wrapper(^^T);

// The arguments that carry a value: the type arguments. A std::array<T, N>
// wraps Ts, not an N.
inline consteval std::vector<std::meta::info>
wrapped_types_of(std::meta::info type) {
    std::vector<std::meta::info> wrapped;
    for (std::meta::info argument :
         std::meta::template_arguments_of(std::meta::dealias(type)))
        if (std::meta::is_type(argument))
            wrapped.push_back(std::meta::remove_cv(argument));
    return wrapped;
}

// Sending a wrapper sends everything it holds. The synchronizable question
// comes first for the same reason it comes first in the structural walk: a type
// that synchronizes itself is sendable whatever it holds, and a rule written as
// a specialization is the only thing standing between that invariant and a
// container someone vouched for.
inline consteval TraitAnswer std_wrapper_is_sendable(std::meta::info type) {
    if (is_synchronizable_type(type))
        return {};

    for (std::meta::info wrapped : wrapped_types_of(type))
        if (!is_sendable_type(wrapped))
            return "wraps a type that is not sendable";

    return {};
}

// [res.on.data.races]: the const member functions of a standard container may
// run concurrently, so a const wrapper is read-safe exactly when everything a
// reader reaches through it — elements and stored policies — is. Reading the
// arguments also keeps the recursion out of libstdc++ internals, whose mutable
// members (unordered_*'s rehash policy) are covered by that guarantee.
inline consteval TraitAnswer
std_wrapper_is_const_synchronizable(std::meta::info type) {
    if (is_synchronizable_type(type))
        return {};

    for (std::meta::info wrapped : wrapped_types_of(type))
        if (!is_synchronizable_type(std::meta::add_const(wrapped)))
            return "wraps a type that is not readable from several threads "
                      "at once";

    return {};
}

// A wrapper owns what it wraps. No borrowed_range test here, unlike the
// structural walk: not one of the templates listed above is a view over
// someone else's storage.
inline consteval TraitAnswer
std_wrapper_is_lifetime_aware(std::meta::info type) {
    for (std::meta::info wrapped : wrapped_types_of(type))
        if (!is_lifetime_aware_type(wrapped))
            return "wraps a type that borrows instead of keeping its data "
                      "alive";

    return {};
}

}

template <detail::std_wrapper T>
struct is_sendable<T> {
    static constexpr TraitAnswer value = detail::std_wrapper_is_sendable(^^T);
};

template <detail::std_wrapper T>
struct is_synchronizable<const T> {
    static constexpr TraitAnswer value = detail::std_wrapper_is_const_synchronizable(^^T);
};

template <detail::std_wrapper T>
struct is_lifetime_aware<T> {
    static constexpr TraitAnswer value = detail::std_wrapper_is_lifetime_aware(^^T);
};

}
