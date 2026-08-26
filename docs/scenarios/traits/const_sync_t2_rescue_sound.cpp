#include <threadsafe/threadsafe.h>
#include <concepts>
#include <string>
#include <type_traits>

// Hijackers whose members are all trivially copyable: the rescue must NOT let
// them through, because the constructor selected for `T b = a;` is user code.
struct GreedyForwardTrivial {
    int *borrowed_;
    GreedyForwardTrivial() = default;
    template <class Argument> GreedyForwardTrivial(Argument &&) : borrowed_(nullptr) {}
};
struct GreedyLvalueTrivial {
    int value_;
    GreedyLvalueTrivial() = default;
    template <class Argument> GreedyLvalueTrivial(Argument &) : value_(0) {}
};
// A hijacker whose constraint admits only its own type: no probe type reaches it.
struct SelfConstrainedHijacker {
    int value_;
    SelfConstrainedHijacker() = default;
    template <class Argument>
        requires std::same_as<std::remove_cvref_t<Argument>, SelfConstrainedHijacker>
    SelfConstrainedHijacker(Argument &&) : value_(99) {}
};
struct GreedyAssignTrivial {
    int value_;
    template <class Argument> GreedyAssignTrivial &operator=(Argument &&) { return *this; }
};
struct UserCopyTrivialMembers {
    int value_;
    UserCopyTrivialMembers(const UserCopyTrivialMembers &other) : value_(other.value_) {}
};

// ground truth: the template really is selected
template <class T> constexpr bool copy_runs_user_code() {
    return !std::is_trivially_constructible_v<T, T &>;
}
static_assert(copy_runs_user_code<GreedyForwardTrivial>(),   "not actually hijacked");
static_assert(copy_runs_user_code<GreedyLvalueTrivial>(),    "not actually hijacked");
static_assert(copy_runs_user_code<SelfConstrainedHijacker>(),"not actually hijacked");

using threadsafe::is_synchronizable_v;
static_assert(!is_synchronizable_v<const GreedyForwardTrivial>,    "GreedyForwardTrivial TRUE");
static_assert(!is_synchronizable_v<const GreedyLvalueTrivial>,     "GreedyLvalueTrivial TRUE");
static_assert(!is_synchronizable_v<const SelfConstrainedHijacker>, "SelfConstrainedHijacker TRUE");
static_assert(!is_synchronizable_v<const GreedyAssignTrivial>,     "GreedyAssignTrivial TRUE");
static_assert(!is_synchronizable_v<const UserCopyTrivialMembers>,  "UserCopyTrivialMembers TRUE");
