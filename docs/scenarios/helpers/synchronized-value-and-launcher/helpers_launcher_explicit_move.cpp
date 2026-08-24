#include <threadsafe/threadsafe.h>
#include <concepts>
namespace {
struct ExplicitMove {
    ExplicitMove() = default;
    ExplicitMove(const ExplicitMove&) = delete;
    explicit ExplicitMove(ExplicitMove&&) = default;
    void operator()() const {}
};
}
static_assert(threadsafe::is_sendable_v<ExplicitMove>);
static_assert(threadsafe::is_lifetime_aware_v<ExplicitMove>);
static_assert(!std::move_constructible<ExplicitMove>);
static_assert(std::constructible_from<ExplicitMove, ExplicitMove&&>,
              "the launcher could construct it in place, the concept still refuses");
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(ExplicitMove{});
}
