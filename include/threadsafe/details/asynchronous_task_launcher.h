#pragma once

#include <concepts>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

namespace detail {

template <class T>
consteval bool diagnose_scoped_task_participant() {
    return std::move_constructible<T> && is_sendable_v<T>;
}

template <class T>
consteval bool diagnose_task_participant() {
    return diagnose_scoped_task_participant<T>() && is_lifetime_aware_v<T>;
}

}

template <class T>
constexpr bool is_scoped_task_participant_v
    = detail::diagnose_scoped_task_participant<T>();

template <class T>
concept scoped_task_participant = is_scoped_task_participant_v<T>;

template <class T>
constexpr bool is_task_participant_v = detail::diagnose_task_participant<T>();

template <class T>
concept task_participant = is_task_participant_v<T>;

template <class F, class... Args>
concept launchable_task = task_participant<F> && (task_participant<Args> && ...);

template <class F, class... Args>
concept launchable_scoped_task = scoped_task_participant<F>
                              && (scoped_task_participant<Args> && ...);

class asynchronous_task_launcher {
    static_assert(task_participant<std::stop_token>,
                  "std::jthread injects a stop_token that the Args constraints "
                  "never see; it must satisfy them on its own");

public:
    template <typename F, typename... Args>
        requires launchable_task<F, Args...>
    void launch_task(F f, Args... args) {
        threads_.emplace_back(std::move(f), std::move(args)...);
    }

    template <typename F, typename... Args>
    void launch_task(F, Args...) {
        static_assert(task_participant<F>,
                      "the callable must be movable, sendable and "
                      "lifetime-aware");
        static_assert((task_participant<Args> && ...),
                      "every argument must be movable, sendable and "
                      "lifetime-aware");
    }

    template <typename F, typename... Args>
        requires launchable_scoped_task<F, Args...>
    void launch_scoped_task(F f, Args... args) {
        std::jthread task{std::move(f), std::move(args)...};
        task.join();
    }

    template <typename F, typename... Args>
    void launch_scoped_task(F, Args...) {
        static_assert(scoped_task_participant<F>,
                      "the callable must be movable and sendable");
        static_assert((scoped_task_participant<Args> && ...),
                      "every argument must be movable and sendable");
    }

private:
    std::vector<std::jthread> threads_;
};

}
