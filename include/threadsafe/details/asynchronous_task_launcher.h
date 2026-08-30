#pragma once

#include <concepts>
#include <meta>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

namespace detail {

template <class T>
consteval TraitAnswer diagnose_scoped_task_participant() {
    if (!std::move_constructible<T>)
        return "cannot be moved, and the launcher owns what it is handed; "
               "share it with std::ref instead";

    return is_sendable_v<T>;
}

template <class T>
consteval TraitAnswer diagnose_task_participant() {
    if (const auto answer = diagnose_scoped_task_participant<T>(); !answer)
        return answer;

    return is_lifetime_aware_v<T>;
}

}

template <class T>
constexpr TraitAnswer is_scoped_task_participant_v
    = detail::diagnose_scoped_task_participant<T>().with_trait(
        "able to take part in a scoped task");

template <class T>
concept scoped_task_participant = bool(is_scoped_task_participant_v<T>);

template <class T>
constexpr TraitAnswer is_task_participant_v
    = detail::diagnose_task_participant<T>().with_trait(
        "able to take part in a task");

template <class T>
concept task_participant = bool(is_task_participant_v<T>);

template <class F, class... Args>
concept launchable_task = task_participant<F> && (task_participant<Args> && ...);

template <class F, class... Args>
concept launchable_scoped_task = scoped_task_participant<F>
                              && (scoped_task_participant<Args> && ...);

namespace detail {

inline consteval std::string_view explain(TraitAnswer answer,
                                          std::meta::info type) {
    if (answer)
        return {};

    const auto rooted = answer.prepend_path(path_step_of_type(type));

    return std::define_static_string(std::string(rooted.full_path())
                                     + " is not " + rooted.trait_name
                                     + " because it " + rooted.error_message);
}

template <class T, const TraitAnswer& answer>
consteval void assert_participant() {
    static_assert(bool(answer), explain(answer, ^^T));
}

}

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
        detail::assert_participant<F, is_task_participant_v<F>>();
        (detail::assert_participant<Args, is_task_participant_v<Args>>(), ...);
    }

    template <typename F, typename... Args>
        requires launchable_scoped_task<F, Args...>
    void launch_scoped_task(F f, Args... args) {
        std::jthread task{std::move(f), std::move(args)...};
        task.join();
    }

    template <typename F, typename... Args>
    void launch_scoped_task(F, Args...) {
        detail::assert_participant<F, is_scoped_task_participant_v<F>>();
        (detail::assert_participant<Args,
                                    is_scoped_task_participant_v<Args>>(),
         ...);
    }

private:
    std::vector<std::jthread> threads_;
};

}
