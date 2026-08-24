#include <threadsafe/threadsafe.h>

#include <barrier>
#include <condition_variable>
#include <functional>
#include <latch>
#include <mutex>
#include <print>
#include <semaphore>
#include <shared_mutex>

namespace threadsafe {
template <> struct is_synchronizable<std::mutex> : std::true_type {};
template <> struct is_synchronizable<std::recursive_mutex> : std::true_type {};
template <> struct is_synchronizable<std::timed_mutex> : std::true_type {};
template <> struct is_synchronizable<std::shared_mutex> : std::true_type {};
template <> struct is_synchronizable<std::shared_timed_mutex> : std::true_type {};
template <> struct is_synchronizable<std::once_flag> : std::true_type {};
template <> struct is_synchronizable<std::condition_variable> : std::true_type {};
template <> struct is_synchronizable<std::condition_variable_any> : std::true_type {};
template <> struct is_synchronizable<std::latch> : std::true_type {};
template <std::ptrdiff_t N>
struct is_synchronizable<std::counting_semaphore<N>> : std::true_type {};
template <class CompletionFunction>
struct is_synchronizable<std::barrier<CompletionFunction>>
    : is_sendable<CompletionFunction> {};

template <> struct is_lifetime_aware<std::mutex> : std::true_type {};
template <> struct is_lifetime_aware<std::latch> : std::true_type {};
}

void worker(std::latch& arrival, std::mutex& guard) {
    std::lock_guard lock{guard};
    arrival.count_down();
}

int main() {
    std::println("is_sendable<std::mutex&>                        = {}", threadsafe::is_sendable_v<std::mutex&>);
    std::println("is_sendable<std::latch&>                        = {}", threadsafe::is_sendable_v<std::latch&>);
    std::println("is_sendable<std::reference_wrapper<std::latch>> = {}",
                 threadsafe::is_sendable_v<std::reference_wrapper<std::latch>>);

    std::latch arrival{1};
    std::mutex guard;
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_scoped_task(&worker, std::ref(arrival), std::ref(guard));
    arrival.wait();
    std::println("scoped task with a shared latch + mutex: launched and joined");
}
