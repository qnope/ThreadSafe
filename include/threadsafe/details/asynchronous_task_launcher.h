#pragma once

#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <threadsafe/details/lifetime_aware.h>
#include <threadsafe/details/sendable.h>

namespace threadsafe {

class asynchronous_task_launcher {
    static_assert(sendable<std::stop_token> && lifetime_aware<std::stop_token>,
                  "std::jthread injects a stop_token that the Args constraints "
                  "never see; it must satisfy them on its own");

public:
    template <typename F, typename... Args>
        requires sendable<F>
              && lifetime_aware<F>
              && (sendable<Args> && ...)
              && (lifetime_aware<Args> && ...)
    void launch_task(F f, Args... args) {
        threads_.emplace_back(std::forward<F>(f), std::forward<Args>(args)...);
    }

    // PRECONDITION: f must not outlive its own invocation — it must not store a
    // reference to any argument beyond the call, nor hand one to a thread it does
    // not itself join. The traits cannot check this; the join bounds the
    // invocation, not the borrow.
    template <typename F, typename... Args>
        requires sendable<F>
              && (sendable<Args> && ...)
    void launch_scoped_task(F f, Args... args) {
        std::jthread task{std::forward<F>(f), std::forward<Args>(args)...};
        task.join();
    }

private:
    std::vector<std::jthread> threads_;
};

}
