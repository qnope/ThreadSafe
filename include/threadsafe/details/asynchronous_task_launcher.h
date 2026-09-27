#pragma once

#include <concepts>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

template <class T>
struct is_allowed_in_task
    : std::conjunction<std::is_move_constructible<T>, is_sendable<T>,
                       is_lifetime_aware<T>> {};

template <class... Args>
constexpr bool is_launchable_task =
    std::conjunction_v<is_allowed_in_task<Args>...>;

template <typename... Ts>
concept launchable_task = is_launchable_task<Ts...>;

class asynchronous_task_launcher {

public:
  template <typename F, typename... Args>
    requires is_launchable_task<F, Args...>
  void launch_task(F f, Args... args) {
    threads_.emplace_back(std::move(f), std::move(args)...);
  }

  template <typename F, typename... Args> void launch_task(F, Args...) {
    static_assert(is_launchable_task<F>,
                  "the callable must be movable, sendable and "
                  "lifetime-aware");
    static_assert((is_launchable_task<Args> && ...),
                  "every argument must be movable, sendable and "
                  "lifetime-aware");
  }

private:
  std::vector<std::jthread> threads_;
};

} // namespace threadsafe
