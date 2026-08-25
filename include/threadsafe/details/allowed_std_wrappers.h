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
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace threadsafe::detail {

// The standard templates that add no state of their own: a specialization is
// nothing but its arguments, held by value. Each of the three traits therefore
// reads straight through to the wrapped types instead of walking the members —
// a std::vector<T> holds a T*, not a T, and its constructor templates block the
// structural default anyway (see may_hijack_copy_move).
//
// An allow-list, not a deduction: nothing in reflection tells std::vector<T>
// apart from a type that hides sharing behind the same arguments. Outside this
// list, the answer is still written as a specialization of the trait.
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

}
