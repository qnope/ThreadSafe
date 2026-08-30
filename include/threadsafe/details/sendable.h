#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/details/synchronizable_base.h>
#include <threadsafe/details/utils.h>

namespace threadsafe {

template <class T>
struct is_unsafe_sendable {};

template <class T>
constexpr TraitAnswer is_unsafe_sendable_v
    = detail::unsafe_answer<is_unsafe_sendable, T>();

inline consteval TraitAnswer is_unsafe_sendable_type(std::meta::info type) {
    return detail::trait_value(^^is_unsafe_sendable_v, type);
}

namespace detail {
consteval TraitAnswer diagnose_is_sendable(std::meta::info type);
}

template <class T>
struct is_sendable {
    static consteval TraitAnswer diagnose() {
        return detail::diagnose_is_sendable(^^T);
    }
};

template <class T>
constexpr TraitAnswer is_sendable_v = is_sendable<T>::diagnose();

template <class T>
struct is_sendable<const T> {
    static consteval TraitAnswer diagnose() {
        return is_sendable_v<std::remove_const_t<T>>;
    }
};

template <class T>
struct is_sendable<T&> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<std::remove_cv_t<T>>;
    }
};
template <class T>
struct is_sendable<T&&> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<std::remove_cv_t<T>>;
    }
};

template <class T>
struct is_sendable<T*> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<std::remove_cv_t<T>>;
    }
};

template <class T, std::size_t N>
struct is_sendable<T[N]> {
    static consteval TraitAnswer diagnose() {
        return is_sendable_v<std::remove_cv_t<T>>;
    }
};
template <class T>
struct is_sendable<T[]> {
    static consteval TraitAnswer diagnose() {
        return is_sendable_v<std::remove_cv_t<T>>;
    }
};

template <class T>
concept sendable = bool(is_sendable_v<T>);

inline consteval TraitAnswer is_sendable_type(std::meta::info type) {
    return detail::trait_value(^^is_sendable_v, type);
}

namespace detail {

inline consteval TraitAnswer diagnose_is_sendable(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();

    if (const auto vouched = is_unsafe_sendable_type(type); vouched.answered)
        return vouched;

    if (is_scalar_type(type) || is_synchronizable_type(type))
        return {};

    if (is_void_type(type))
        return "void holds no value to send";

    if (!is_class_type(type) && !is_union_type(type))
        return "is not a scalar, class or union type — is_sendable<T> "
                  "supports no others";

    if (!is_complete_type(type))
        return "is incomplete — is_sendable<T> needs a complete type; "
                  "specialize is_sendable for a type holding a pointer to an "
                  "incomplete type (the pimpl idiom)";

    if (const auto answer = is_default_type(type); !answer)
        return answer;

    if (has_unreflectable_state(type))
        return "holds state reflection cannot see (a closure type with "
                  "captures); specialize is_sendable to state the intent";

    for (info base : bases_of(type, context))
        if (const auto answer = is_sendable_type(type_of(base)); !answer)
            return "a base class is not sendable";

    for (info member : nonstatic_data_members_of(type, context))
        if (const auto answer = is_sendable_type(remove_cv(type_of(member)));
            !answer)
            return "a member is not sendable";

    return {};
}

}

}
