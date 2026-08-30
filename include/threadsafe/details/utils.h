#pragma once

#include <cstddef>
#include <meta>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

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

    constexpr std::span<const char *const> paths() const {
        return {path_steps, path_step_count};
    }

    consteval TraitAnswer prepend_path(std::string_view step) const {
        if (error_message == nullptr)
            return *this;

        std::vector<const char *> extended_path;
        extended_path.reserve(path_step_count + 1);
        extended_path.push_back(std::define_static_string(step));
        extended_path.insert(extended_path.end(), path_steps,
                             path_steps + path_step_count);

        const auto promoted_path = std::define_static_array(extended_path);

        TraitAnswer answer = *this;
        answer.path_steps = promoted_path.data();
        answer.path_step_count = promoted_path.size();
        return answer;
    }

    consteval TraitAnswer with_trait(std::string_view asking_trait) const {
        if (error_message == nullptr || trait_name != nullptr)
            return *this;

        TraitAnswer answer = *this;
        answer.trait_name = std::define_static_string(asking_trait);
        return answer;
    }

    consteval std::string_view full_path() const {
        std::string joined_path;
        for (const char *step : paths()) {
            if (!joined_path.empty())
                joined_path += "::";
            joined_path += step;
        }
        return std::define_static_string(joined_path);
    }

    const char *error_message = nullptr;
    const char *trait_name = nullptr;
    const char *const *path_steps = nullptr;
    std::size_t path_step_count = 0;
    bool answered = true;
};

}

namespace threadsafe::detail {

inline constexpr std::string_view pointee_step = "*";
inline constexpr std::string_view referent_step = "&";
inline constexpr std::string_view element_step = "[]";

inline consteval std::string_view path_step_of_type(std::meta::info type) {
    if (std::meta::has_identifier(type))
        return std::meta::identifier_of(type);
    return std::meta::display_string_of(type);
}

inline consteval std::string_view path_step_of_base(std::meta::info base_type) {
    return std::define_static_string(
        "base (" + std::string(path_step_of_type(base_type)) + ")");
}

inline consteval std::string_view path_step_of_member(std::meta::info member) {
    const std::string_view name = std::meta::has_identifier(member)
                                    ? std::meta::identifier_of(member)
                                    : "(anonymous)";

    return std::define_static_string(
        std::string(name) + " ("
        + std::string(path_step_of_type(std::meta::type_of(member))) + ")");
}

template <class AskBase, class AskMember>
consteval TraitAnswer walk_bases_and_members(std::meta::info type,
                                             AskBase ask_base,
                                             AskMember ask_member) {
    const auto context = std::meta::access_context::unchecked();

    for (std::meta::info base : std::meta::bases_of(type, context))
        if (const auto answer = ask_base(base); !answer)
            return answer.prepend_path(
                path_step_of_base(std::meta::type_of(base)));

    for (std::meta::info member :
         std::meta::nonstatic_data_members_of(type, context))
        if (const auto answer = ask_member(member); !answer)
            return answer.prepend_path(path_step_of_member(member));

    return {};
}

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
            return "has a constructor or assignment template that may be "
                      "selected as a copy or a move; write the special "
                      "members out, or specialize the trait";

        if (!is_copy_move_destroy_member(member))
            continue;

        if (!std::meta::is_defaulted(member) && !std::meta::is_deleted(member))
            return "has a user-written copy, move or destructor that can "
                      "share state the members do not show; specialize the "
                      "trait to state the intent";
    }

    return {};
}

}
