#pragma once

#include <meta>
#include <type_traits>

namespace threadsafe::detail {

inline consteval bool trait_value(std::meta::info trait, std::meta::info type) {
  return extract<bool>(substitute(trait, {type}));
}

template <typename T> struct is_smart_pointer : std::false_type {};

template <typename T>
struct is_smart_pointer<std::shared_ptr<T>> : std::true_type {};

template <typename T>
struct is_smart_pointer<std::weak_ptr<T>> : std::true_type {};

template <typename T>
struct is_smart_pointer<std::unique_ptr<T>> : std::true_type {};

template <typename T>
constexpr bool is_smart_pointer_v = is_smart_pointer<T>::value;

template <class T>
concept smart_pointer = is_smart_pointer<T>::value;

inline consteval bool is_smart_pointer_type(std::meta::info info) {
  return trait_value(^^is_smart_pointer_v, info);
}

template <class T> consteval bool compute_dynamic_type_is_known() {
  if constexpr (std::is_void_v<T>)
    return false;
  else if constexpr (!is_complete_type(^^T))
    return false;
  else if constexpr (std::is_polymorphic_v<T> && !std::is_final_v<T>)
    return false;
  else
    return true;
}

template <class T>
constexpr bool dynamic_type_is_known = compute_dynamic_type_is_known<T>();

inline consteval bool has_unreflectable_state(std::meta::info type) {
  const auto context = std::meta::access_context::unchecked();
  return !is_empty_type(type) && !is_polymorphic_type(type) &&
         bases_of(type, context).empty() &&
         nonstatic_data_members_of(type, context).empty();
}

inline consteval bool all_bases_and_members(std::meta::info type,
                                            bool (*question)(std::meta::info)) {
  const auto context = std::meta::access_context::unchecked();

  for (auto base : bases_of(type, context))
    if (!question(type_of(base)))
      return false;

  for (auto member : nonstatic_data_members_of(type, context))
    if (!question(remove_cv(type_of(member))))
      return false;

  return true;
}

inline consteval bool is_copy_move_destructor(std::meta::info function) {
  if (is_copy_constructor(function))
    return true;
  if (is_move_constructor(function))
    return true;
  if (is_copy_assignment(function))
    return true;
  if (is_move_assignment(function))
    return true;
  return is_destructor(function);
}

inline consteval bool may_hijack_copy_move(std::meta::info function) {
  if (is_constructor_template(function))
    return true;

  if (is_operator_function_template(function))
    return operator_of(function) == std::meta::op_equals;

  return false;
}

inline consteval bool is_default_type(std::meta::info type) {
  const auto context = std::meta::access_context::unchecked();

  for (auto member : std::meta::members_of(type, context)) {
    if (may_hijack_copy_move(member))
      return false;

    if (is_copy_move_destructor(member) && !is_defaulted(member) &&
        !is_deleted(member))
      return false;
  }

  return true;
}

inline consteval bool is_walkable_type(std::meta::info type) {
  if (!is_class_type(type) && !is_union_type(type))
    return false;

  if (!is_complete_type(type))
    return false;

  if (!is_default_type(type))
    return false;

  return !has_unreflectable_state(type);
}

} // namespace threadsafe::detail
