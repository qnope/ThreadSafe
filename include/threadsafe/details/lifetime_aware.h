#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <meta>
#include <ranges>
#include <type_traits>

#include <threadsafe/details/utils.h>

namespace threadsafe {

template <class T>
struct is_unsafe_lifetime_aware {};

template <class T>
constexpr TraitAnswer is_unsafe_lifetime_aware_v
    = detail::unsafe_answer<is_unsafe_lifetime_aware, T>();

inline consteval TraitAnswer
is_unsafe_lifetime_aware_type(std::meta::info type) {
    return detail::trait_value(^^is_unsafe_lifetime_aware_v, type);
}

namespace detail {
consteval TraitAnswer diagnose_is_lifetime_aware(std::meta::info type);
}

template <class T>
struct is_lifetime_aware
{
    static consteval TraitAnswer diagnose() {
        return detail::diagnose_is_lifetime_aware(^^T);
    }
};

template <class T>
constexpr TraitAnswer is_lifetime_aware_v = is_lifetime_aware<T>::diagnose();

template <class T>
concept lifetime_aware = bool(is_lifetime_aware_v<T>);

inline consteval TraitAnswer is_lifetime_aware_type(std::meta::info type) {
    return detail::trait_value(^^is_lifetime_aware_v, type);
}

template <class T>
struct is_unsafe_lifetime_aware<T&> {
    static consteval TraitAnswer diagnose() {
        return "References borrow their referent instead of keeping it alive";
    }
};

template <class T>
struct is_unsafe_lifetime_aware<T&&> {
    static consteval TraitAnswer diagnose() {
        return "References borrow their referent instead of keeping it alive";
    }
};

template <class T>
struct is_unsafe_lifetime_aware<T*> {
    static consteval TraitAnswer diagnose() {
        return "Raw pointers borrow their pointee instead of keeping it alive";
    }
};

template <class F>
    requires std::is_function_v<F>
struct is_unsafe_lifetime_aware<F*> {
    static consteval TraitAnswer diagnose() { return {}; }
};

template <class T, std::size_t N>
struct is_unsafe_lifetime_aware<T[N]> {
    static consteval TraitAnswer diagnose() {
        return is_lifetime_aware_v<std::remove_cv_t<T>>.prepend_path(
            detail::element_step);
    }
};

template <class T>
struct is_unsafe_lifetime_aware<T[]> {
    static consteval TraitAnswer diagnose() {
        return is_lifetime_aware_v<std::remove_cv_t<T>>.prepend_path(
            detail::element_step);
    }
};

template <class T>
struct is_unsafe_lifetime_aware<std::reference_wrapper<T>> {
    static consteval TraitAnswer diagnose() {
        return "std::reference_wrapper borrows its referent instead of "
               "keeping it alive";
    }
};

template <class T>
struct is_unsafe_lifetime_aware<std::shared_ptr<T>> {
    using pointee = std::remove_cv_t<std::remove_all_extents_t<T>>;

    static consteval TraitAnswer diagnose() {
        if (const auto answer = is_lifetime_aware_v<pointee>; !answer)
            return answer.prepend_path(detail::pointee_step);

        return detail::dynamic_type_is_known<pointee>;
    }
};

template <class T>
struct is_unsafe_lifetime_aware<std::weak_ptr<T>> {
    using pointee = std::remove_cv_t<std::remove_all_extents_t<T>>;

    static consteval TraitAnswer diagnose() {
        if (const auto answer = is_lifetime_aware_v<pointee>; !answer)
            return answer.prepend_path(detail::pointee_step);

        return detail::dynamic_type_is_known<pointee>;
    }
};

namespace detail {

inline consteval TraitAnswer diagnose_is_lifetime_aware(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    const auto unqualified = remove_cv(type);

    if (unqualified != type)
        return is_lifetime_aware_type(unqualified);

    if (const auto vouched = is_unsafe_lifetime_aware_type(type);
        vouched.answered)
        return vouched;

    if (is_void_type(type))
        return "void holds no value to own";

    if (extract<bool>(substitute(^^std::ranges::borrowed_range, {type})))
        return "is a borrowed range: a view over someone else's storage, it "
                  "does not keep its elements alive";

    if (!is_class_type(type) && !is_union_type(type))
        return {};

    if (!is_complete_type(type))
        return "is incomplete — is_lifetime_aware<T> needs a complete "
                  "type";

    if (has_unreflectable_state(type))
        return "holds state reflection cannot see (a closure type with "
                  "captures); specialize is_unsafe_lifetime_aware to state "
                  "the intent";

    for (info base : bases_of(type, context))
        if (const auto answer = is_lifetime_aware_type(type_of(base)); !answer)
            return answer.prepend_path(path_step_of_type(type_of(base)));

    for (info member : nonstatic_data_members_of(type, context))
        if (const auto answer
            = is_lifetime_aware_type(remove_cv(type_of(member)));
            !answer)
            return answer.prepend_path(path_step_of_member(member));

    return {};
}

}

}
