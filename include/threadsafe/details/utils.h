#pragma once

#include <meta>
#include <string_view>
#include <type_traits>

namespace threadsafe {

struct TraitAnswer {
    constexpr TraitAnswer() = default;

    consteval TraitAnswer(const char *reason)
        : error_message(std::define_static_string(std::string_view(reason))) {}

    static constexpr TraitAnswer unanswered() {
        TraitAnswer answer;
        answer.answered = false;
        return answer;
    }

    constexpr explicit operator bool() const {
        return answered && error_message == nullptr;
    }

    const char *error_message = nullptr;
    bool answered = true;
};

}

namespace threadsafe::detail {

inline consteval TraitAnswer trait_value(std::meta::info trait,
                                         std::meta::info type) {
    return std::meta::extract<TraitAnswer>(std::meta::substitute(trait, {type}));
}

template <template <class> class UnsafeTrait, class T>
consteval TraitAnswer unsafe_answer() {
    if constexpr (requires { UnsafeTrait<T>::diagnose(); })
        return UnsafeTrait<T>::diagnose();
    else {
        static_assert(!requires { UnsafeTrait<T>::value; },
                      "an is_unsafe_<trait> specialization states its claim as "
                      "`static consteval TraitAnswer diagnose()`, not as a "
                      "`value` member");
        return TraitAnswer::unanswered();
    }
}

template <class T>
consteval TraitAnswer compute_dynamic_type_is_known() {
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
