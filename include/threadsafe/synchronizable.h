#pragma once

#include <atomic>
#include <type_traits>

#include <threadsafe/sendable.h>
#include <threadsafe/synchronizable_base.h>

namespace threadsafe {

// Code is immutable: a function may be called from any number of threads.
template <class F>
concept function_type = std::is_function_v<F>;

template <function_type F>
constexpr bool is_synchronizable<F> = true;

// std::atomic<T> synchronizes every access to its value, so sharing it across
// threads is safe exactly when handing a T to another thread would be.
template <class T>
constexpr bool is_synchronizable<std::atomic<T>> = is_sendable<T>;

}  // namespace threadsafe
