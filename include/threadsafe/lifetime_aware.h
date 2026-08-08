#pragma once

#include <functional>
#include <memory>
#include <ranges>

namespace threadsafe {

// True if a T owns its data or keeps its referent alive.
// By value: true, except borrowed ranges (views over data they don't own).
// Queries must decay T first; cv-qualified queries are not forwarded.
template <class T>
constexpr bool is_lifetime_aware = !std::ranges::borrowed_range<T>;

// References and pointers do not keep their referent alive.
template <class T>
constexpr bool is_lifetime_aware<T&> = false;
template <class T>
constexpr bool is_lifetime_aware<T&&> = false;
template <class T>
constexpr bool is_lifetime_aware<T*> = false;
template <class T>
constexpr bool is_lifetime_aware<std::reference_wrapper<T>> = false;

template <class T>
constexpr bool is_lifetime_aware<std::shared_ptr<T>> = true;
template <class T>
constexpr bool is_lifetime_aware<std::weak_ptr<T>> = true;

template <class T>
concept lifetime_aware = is_lifetime_aware<T>;

}  // namespace threadsafe
