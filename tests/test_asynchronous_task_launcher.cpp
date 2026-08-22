#include <threadsafe/threadsafe.h>

#include <functional>
#include <memory>
#include <string>

namespace {
struct SyncCounter {
    std::atomic<int> counter{0};
    void operator()() const {}
};
struct NonSendable {
    NonSendable(NonSendable const&) {}
};

template <class F, class... Args>
constexpr bool can_launch_task =
    requires(threadsafe::asynchronous_task_launcher l, F f, Args... args) {
        l.launch_task(f, args...);
    };

template <class F, class... Args>
constexpr bool can_launch_scoped_task =
    requires(threadsafe::asynchronous_task_launcher l, F f, Args... args) {
        l.launch_scoped_task(f, args...);
    };
}

template <>
constexpr bool threadsafe::is_synchronizable<SyncCounter> = true;

static_assert(can_launch_task<decltype([] {})>,
              "launch_task — a captureless lambda with no args is accepted");
static_assert(can_launch_task<decltype([](int, std::string) {}),
                              int, std::string>,
              "launch_task — sendable, lifetime-aware args are accepted");
static_assert(can_launch_task<decltype([](std::shared_ptr<SyncCounter>) {}),
                              std::shared_ptr<SyncCounter>>,
              "launch_task — a shared_ptr keeps its referent alive and the"
              " referent is synchronizable");

static_assert(!can_launch_task<decltype([](SyncCounter&) {}),
                               std::reference_wrapper<SyncCounter>>,
              "launch_task — a reference_wrapper does not keep its referent"
              " alive");
static_assert(!can_launch_task<decltype([](SyncCounter*) {}), SyncCounter*>,
              "launch_task — a raw pointer does not keep its pointee alive");

static_assert(can_launch_scoped_task<decltype([](SyncCounter&) {}),
                                     std::reference_wrapper<SyncCounter>>,
              "launch_scoped_task — the launcher waits for the task, so a"
              " reference to a synchronizable object may cross");
static_assert(!can_launch_scoped_task<decltype([](std::string&) {}),
                                      std::reference_wrapper<std::string>>,
              "launch_scoped_task — sharing a non-synchronizable referent is"
              " still forbidden");

static_assert(!can_launch_task<decltype([x = 42] {})>,
              "launch_task — a capturing lambda is not a safe callable");
static_assert(!can_launch_scoped_task<std::function<void()>>,
              "launch_scoped_task — std::function owns unsynchronized state");
static_assert(can_launch_scoped_task<std::reference_wrapper<SyncCounter>>,
              "launch_scoped_task — a synchronizable callable may be shared by"
              " reference, since the launcher waits for the task");
static_assert(!can_launch_task<std::reference_wrapper<SyncCounter>>,
              "launch_task — the callable must keep itself alive too, and a"
              " reference_wrapper does not");
static_assert(!can_launch_task<SyncCounter>,
              "launch_task — the launcher owns its callable, so a non-copyable"
              " one cannot cross; share it with std::ref instead");

static_assert(!can_launch_task<decltype([](NonSendable) {}), NonSendable>,
              "launch_task — a user-provided copy constructor could share"
              " state, so the arg is not sendable");
static_assert(!can_launch_scoped_task<decltype([](NonSendable) {}),
                                      NonSendable>,
              "launch_scoped_task — non-sendable args are rejected even when"
              " scoped");
