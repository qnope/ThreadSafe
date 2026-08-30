#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>

#include <threadsafe/details/utils.h>

namespace threadsafe {

template <class T>
struct is_unsafe_synchronizable {};

template <class T>
constexpr TraitAnswer is_unsafe_synchronizable_v
    = detail::unsafe_answer<is_unsafe_synchronizable, T>();

inline consteval TraitAnswer
is_unsafe_synchronizable_type(std::meta::info type) {
    return detail::trait_value(^^is_unsafe_synchronizable_v, type);
}

namespace detail {

consteval TraitAnswer diagnose_is_const_synchronizable(std::meta::info type);

inline consteval TraitAnswer diagnose_is_synchronizable(std::meta::info type) {
    if (const auto vouched = is_unsafe_synchronizable_type(type);
        vouched.answered)
        return vouched;

    return "is_unsafe_synchronizable<T> is opt-in: specialize it to vouch for "
           "a type that carries its own synchronization";
}

}

template <class T>
struct is_synchronizable {
    static consteval TraitAnswer diagnose() {
        return detail::diagnose_is_synchronizable(^^T);
    }
};

template <class T>
constexpr TraitAnswer is_synchronizable_v = is_synchronizable<T>::diagnose();

inline consteval TraitAnswer is_synchronizable_type(std::meta::info type) {
    return detail::trait_value(^^is_synchronizable_v, type);
}

template <class T>
struct is_unsafe_synchronizable<const T> {
    static consteval TraitAnswer diagnose() {
        return detail::diagnose_is_const_synchronizable(^^T);
    }
};

template <class T, std::size_t N>
struct is_unsafe_synchronizable<T[N]> {
    static consteval TraitAnswer diagnose() { return is_synchronizable_v<T>; }
};

template <class T>
struct is_unsafe_synchronizable<T[]> {
    static consteval TraitAnswer diagnose() { return is_synchronizable_v<T>; }
};

template <class T, std::size_t N>
struct is_unsafe_synchronizable<const T[N]> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<const T>;
    }
};

template <class T>
struct is_unsafe_synchronizable<const T[]> {
    static consteval TraitAnswer diagnose() {
        return is_synchronizable_v<const T>;
    }
};

namespace detail {

inline consteval TraitAnswer
diagnose_is_const_synchronizable(std::meta::info type) {
    using namespace std::meta;

    const auto context = access_context::unchecked();
    type = remove_cv(type);

    if (const auto vouched = is_unsafe_synchronizable_type(type);
        vouched.answered)
        return vouched;

    if (is_pointer_type(type)) {
        if (!is_synchronizable_type(remove_cv(remove_pointer(type))))
            return "is a pointer: the const stops at it — the pointee may "
                      "be written through another alias, so the pointee must "
                      "be synchronizable itself";
        return {};
    }

    if (is_scalar_type(type))
        return {};

    if (is_void_type(type))
        return "void holds no value to read";

    if (!is_class_type(type) && !is_union_type(type))
        return "is not a scalar, class or union type — "
                  "is_synchronizable<const T> supports no others";

    if (!is_complete_type(type))
        return "is incomplete — is_synchronizable<const T> needs a complete "
                  "type; specialize is_unsafe_synchronizable for a type "
                  "holding a pointer to an incomplete type (the pimpl idiom)";

    if (const auto answer = is_default_type(type); !answer)
        return answer;

    if (has_unreflectable_state(type))
        return "holds state reflection cannot see (a closure type with "
                  "captures); specialize is_unsafe_synchronizable to state "
                  "the intent";

    for (info base : bases_of(type, context))
        if (!is_synchronizable_type(add_const(type_of(base))))
            return "a base class is not readable from several threads at "
                      "once";

    for (info member : nonstatic_data_members_of(type, context)) {
        const auto member_type = type_of(member);

        if (is_mutable_member(member)) {
            if (!is_synchronizable_type(remove_cv(member_type)))
                return "a mutable member is written through a const "
                          "reference: its type must be fully synchronizable";
        }
        else if (is_reference_type(member_type)) {
            if (!is_synchronizable_type(remove_cvref(member_type)))
                return "a reference member stops the const: its referent "
                          "must be synchronizable itself";
        }
        else if (!is_synchronizable_type(add_const(member_type))) {
            return "a member is not readable from several threads at once";
        }
    }

    return {};
}

}

}
