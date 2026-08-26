#include <threadsafe/threadsafe.h>

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct Blessed { int value = 0; };
struct Plain   { int value = 0; };

template <class F, class... Args>
constexpr bool can_launch_task = threadsafe::launchable_task<F, Args...>;
template <class F, class... Args>
constexpr bool can_launch_scoped = threadsafe::launchable_scoped_task<F, Args...>;

void takes_ptr(Blessed*) {}
void takes_ref(Blessed&) {}
void takes_plain_ref(Plain&) {}
void takes_sv(std::string_view) {}
void takes_span(std::span<int>) {}
void takes_vec_ptr(std::vector<Blessed*>) {}
void takes_unique(std::unique_ptr<Plain>) {}
void takes_shared_blessed(std::shared_ptr<Blessed>) {}
void takes_shared_plain(std::shared_ptr<Plain>) {}
void takes_string(std::string) {}
}

THREADSAFE_UNSAFE_ASSERT_SYNCHRONIZABLE(Blessed);

using threadsafe::is_sendable_v;
using threadsafe::is_lifetime_aware_v;

// raw pointer to a BLESSED type: sendable but not lifetime aware
static_assert(is_sendable_v<Blessed*>,       "POLARITY: blessed pointer is sendable");
static_assert(!is_lifetime_aware_v<Blessed*>);
static_assert(!can_launch_task<void(*)(Blessed*), Blessed*>,   "launch_task rejects a raw ptr");
static_assert(can_launch_scoped<void(*)(Blessed*), Blessed*>,  "POLARITY: scoped accepts a raw ptr to a blessed type");

// raw pointer to a NON blessed type: not even sendable
static_assert(!is_sendable_v<Plain*>);
static_assert(!can_launch_scoped<void(*)(Plain*), Plain*>);

// std::ref: reference_wrapper<T> needs Sync
static_assert(is_sendable_v<std::reference_wrapper<Blessed>>,  "POLARITY: ref to blessed sendable");
static_assert(!is_sendable_v<std::reference_wrapper<Plain>>);
static_assert(!is_lifetime_aware_v<std::reference_wrapper<Blessed>>);
static_assert(!can_launch_task<void(*)(Blessed&), std::reference_wrapper<Blessed>>);
static_assert(can_launch_scoped<void(*)(Blessed&), std::reference_wrapper<Blessed>>,
              "POLARITY: scoped accepts std::ref to a blessed type");
static_assert(!can_launch_scoped<void(*)(Plain&), std::reference_wrapper<Plain>>);

// string_view / span: not even sendable, so a SCOPED task cannot take one either
static_assert(!is_sendable_v<std::string_view>,      "POLARITY: string_view not sendable");
static_assert(!is_lifetime_aware_v<std::string_view>);
static_assert(!can_launch_scoped<void(*)(std::string_view), std::string_view>,
              "a scoped task cannot even borrow a string_view");
static_assert(!is_sendable_v<std::span<int>>,        "POLARITY: span not sendable");
static_assert(!can_launch_scoped<void(*)(std::span<int>), std::span<int>>);

// vector of pointers
static_assert(is_sendable_v<std::vector<Blessed*>>);
static_assert(!is_lifetime_aware_v<std::vector<Blessed*>>);
static_assert(!can_launch_task<void(*)(std::vector<Blessed*>), std::vector<Blessed*>>);
static_assert(can_launch_scoped<void(*)(std::vector<Blessed*>), std::vector<Blessed*>>,
              "POLARITY: scoped accepts a vector of blessed pointers");

// unique_ptr / shared_ptr
static_assert(can_launch_task<void(*)(std::unique_ptr<Plain>), std::unique_ptr<Plain>>,
              "POLARITY: unique_ptr transfers ownership, accepted");
static_assert(can_launch_task<void(*)(std::shared_ptr<Blessed>), std::shared_ptr<Blessed>>,
              "POLARITY: shared_ptr to a blessed type accepted");
static_assert(!can_launch_task<void(*)(std::shared_ptr<Plain>), std::shared_ptr<Plain>>,
              "a shared_ptr to a non-synchronizable type is not sendable");
static_assert(!can_launch_scoped<void(*)(std::shared_ptr<Plain>), std::shared_ptr<Plain>>,
              "POLARITY: not even scoped");

// std::string by value
static_assert(can_launch_task<void(*)(std::string), std::string>, "POLARITY: string accepted");

// string literal
static_assert(!can_launch_task<void(*)(std::string_view), const char*>);
static_assert(!can_launch_scoped<void(*)(std::string_view), const char*>,
              "not even a string literal may cross into a scoped task");

int main() {}
