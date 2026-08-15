#pragma once

#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <threadsafe/lifetime_aware.h>
#include <threadsafe/safe_callable.h>
#include <threadsafe/sendable.h>

namespace threadsafe {

class asynchronous_task_launcher {
    static_assert(sendable<std::stop_token> && lifetime_aware<std::stop_token>,
                  "std::jthread injects a stop_token that the Args constraints "
                  "never see; it must satisfy them on its own");

public:
    template <typename F, typename... Args>
        requires safe_callable<std::decay_t<F>>
              && sendable<std::decay_t<F>>
              && lifetime_aware<std::decay_t<F>>
              && (sendable<std::decay_t<Args>> && ...)
              && (lifetime_aware<std::decay_t<Args>> && ...)
    void launch_task(F&& f, Args&&... args) {
        threads_.emplace_back(std::forward<F>(f), std::forward<Args>(args)...);
    }

    template <typename F, typename... Args>
        requires safe_callable<std::decay_t<F>>
              && sendable<std::decay_t<F>>
              && (sendable<std::decay_t<Args>> && ...)
    void launch_scoped_task(F&& f, Args&&... args) {
        std::jthread task{std::forward<F>(f), std::forward<Args>(args)...};
        task.join();
    }

private:
    std::vector<std::jthread> threads_;
};

}
